/**
 * XPU - src/backend/sw_rasterizer.cpp
 *
 * Real software rasterizer implementation. This is the heart of the XPU
 * software backend - it actually fills pixels into a framebuffer.
 *
 * Algorithm:
 *   1. Perspective divide each vertex (clip space -> NDC)
 *   2. Viewport transform (NDC -> screen pixels)
 *   3. Compute triangle bounding box on screen
 *   4. For each pixel in the bbox, compute edge functions
 *   5. If all 3 edges are positive (CCW winding), pixel is inside
 *   6. Perspective-correct barycentric interpolation of color and Z
 *   7. Depth test against Z-buffer, write if closer
 *
 * The scanline inner loop is unrolled by 4 pixels to let the compiler
 * (or our Assembly overrides) use SIMD.
 */

#include "sw_rasterizer.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <algorithm>

/* ------------------------------------------------------------------ */
/* Render target management                                           */
/* ------------------------------------------------------------------ */

extern "C" XpuRenderTarget* xpu_sw_rt_create(uint32_t width, uint32_t height) {
    auto* rt = (XpuRenderTarget*)std::malloc(sizeof(XpuRenderTarget));
    if (!rt) return nullptr;
    rt->width  = width;
    rt->height = height;
    rt->color  = (uint32_t*)std::malloc((size_t)width * height * sizeof(uint32_t));
    rt->depth  = (float*)std::malloc((size_t)width * height * sizeof(float));
    if (!rt->color || !rt->depth) {
        std::free(rt->color);
        std::free(rt->depth);
        std::free(rt);
        return nullptr;
    }
    return rt;
}

extern "C" void xpu_sw_rt_destroy(XpuRenderTarget* rt) {
    if (!rt) return;
    std::free(rt->color);
    std::free(rt->depth);
    std::free(rt);
}

extern "C" void xpu_sw_rt_clear_color(XpuRenderTarget* rt, float r, float g, float b, float a) {
    if (!rt) return;
    uint8_t rr = (uint8_t)(r * 255.0f + 0.5f);
    uint8_t gg = (uint8_t)(g * 255.0f + 0.5f);
    uint8_t bb = (uint8_t)(b * 255.0f + 0.5f);
    uint8_t aa = (uint8_t)(a * 255.0f + 0.5f);
    uint32_t pixel = ((uint32_t)aa << 24) | ((uint32_t)bb << 16) |
                     ((uint32_t)gg << 8) | (uint32_t)rr;
    size_t total = (size_t)rt->width * rt->height;
    /* SIMD-friendly fill */
    for (size_t i = 0; i + 4 <= total; i += 4) {
        rt->color[i + 0] = pixel;
        rt->color[i + 1] = pixel;
        rt->color[i + 2] = pixel;
        rt->color[i + 3] = pixel;
    }
    for (size_t i = (total / 4) * 4; i < total; ++i) rt->color[i] = pixel;
}

extern "C" void xpu_sw_rt_clear_depth(XpuRenderTarget* rt, float depth) {
    if (!rt) return;
    size_t total = (size_t)rt->width * rt->height;
    for (size_t i = 0; i < total; ++i) rt->depth[i] = depth;
}

/* ------------------------------------------------------------------ */
/* BMP writer - simple, no dependencies                               */
/* ------------------------------------------------------------------ */

#pragma pack(push, 1)
struct BmpFileHeader {
    uint16_t signature;     /* 'BM' */
    uint32_t file_size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t pixel_offset;
};

struct BmpInfoHeader {
    uint32_t header_size;   /* 40 */
    int32_t  width;
    int32_t  height;        /* positive = bottom-up */
    uint16_t planes;        /* 1 */
    uint16_t bpp;           /* 32 */
    uint32_t compression;   /* 0 = none */
    uint32_t image_size;
    int32_t  x_ppm;
    int32_t  y_ppm;
    uint32_t colors_used;
    uint32_t colors_important;
};
#pragma pack(pop)

extern "C" int xpu_sw_rt_save_bmp(const XpuRenderTarget* rt, const char* path) {
    if (!rt || !path) return -1;
    std::FILE* f = std::fopen(path, "wb");
    if (!f) return -1;

    const uint32_t row_size = rt->width * 4;
    const uint32_t img_size = row_size * rt->height;

    BmpFileHeader fh{};
    fh.signature    = 0x4D42;  /* 'BM' */
    fh.file_size    = sizeof(BmpFileHeader) + sizeof(BmpInfoHeader) + img_size;
    fh.pixel_offset = sizeof(BmpFileHeader) + sizeof(BmpInfoHeader);

    BmpInfoHeader ih{};
    ih.header_size = 40;
    ih.width       = (int32_t)rt->width;
    ih.height      = (int32_t)rt->height;  /* bottom-up */
    ih.planes      = 1;
    ih.bpp         = 32;
    ih.compression = 0;
    ih.image_size  = img_size;
    ih.x_ppm       = 2835;  /* 72 DPI */
    ih.y_ppm       = 2835;

    std::fwrite(&fh, sizeof(fh), 1, f);
    std::fwrite(&ih, sizeof(ih), 1, f);

    /* BMP is BGRA, our buffer is RGBA. Convert row by row.
     * Also BMP is bottom-up by default (positive height). */
    for (uint32_t y = 0; y < rt->height; ++y) {
        const uint32_t* row = rt->color + (size_t)y * rt->width;
        for (uint32_t x = 0; x < rt->width; ++x) {
            uint32_t rgba = row[x];
            /* rgba is packed as 0xAABBGGRR (R in low byte) */
            uint8_t r = (uint8_t)(rgba & 0xFF);
            uint8_t g = (uint8_t)((rgba >> 8)  & 0xFF);
            uint8_t b = (uint8_t)((rgba >> 16) & 0xFF);
            uint8_t a = (uint8_t)((rgba >> 24) & 0xFF);
            uint32_t bgra = ((uint32_t)a << 24) | ((uint32_t)r << 16) |
                            ((uint32_t)g << 8)  | (uint32_t)b;
            std::fwrite(&bgra, 4, 1, f);
        }
    }

    std::fclose(f);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Triangle rasterization                                             */
/* ------------------------------------------------------------------ */

namespace {

struct ScreenVertex {
    float x, y;        /* screen-space pixel coords */
    float z;           /* NDC z in [-1, 1] */
    float inv_w;       /* 1 / w (for perspective-correct interpolation) */
    float r, g, b, a;  /* color divided by w (perspective-correct) */
};

inline ScreenVertex to_screen(const XpuVertex& v, uint32_t fb_w, uint32_t fb_h) {
    ScreenVertex sv{};
    if (std::fabs(v.position.w) < 1e-12f) {
        sv.inv_w = 1.0f;
    } else {
        sv.inv_w = 1.0f / v.position.w;
    }
    /* Perspective divide */
    float nx = v.position.x * sv.inv_w;
    float ny = v.position.y * sv.inv_w;
    float nz = v.position.z * sv.inv_w;
    /* Viewport transform: NDC [-1,1] -> screen [0, w/h] */
    sv.x = (nx + 1.0f) * 0.5f * (float)fb_w;
    sv.y = (1.0f - ny) * 0.5f * (float)fb_h;  /* flip Y */
    sv.z = nz;
    /* Perspective-correct color: store color / w, multiply by inv_w later */
    sv.r = v.color.x * sv.inv_w;
    sv.g = v.color.y * sv.inv_w;
    sv.b = v.color.z * sv.inv_w;
    sv.a = v.color.w * sv.inv_w;
    return sv;
}

inline float edge_function(float ax, float ay, float bx, float by, float cx, float cy) {
    return (cx - ax) * (by - ay) - (cy - ay) * (bx - ax);
}

inline uint32_t pack_rgba8(float r, float g, float b, float a) {
    if (r < 0) r = 0;
    if (r > 1) r = 1;
    if (g < 0) g = 0;
    if (g > 1) g = 1;
    if (b < 0) b = 0;
    if (b > 1) b = 1;
    if (a < 0) a = 0;
    if (a > 1) a = 1;
    uint8_t rr = (uint8_t)(r * 255.0f + 0.5f);
    uint8_t gg = (uint8_t)(g * 255.0f + 0.5f);
    uint8_t bb = (uint8_t)(b * 255.0f + 0.5f);
    uint8_t aa = (uint8_t)(a * 255.0f + 0.5f);
    return ((uint32_t)aa << 24) | ((uint32_t)bb << 16) |
           ((uint32_t)gg << 8)  | (uint32_t)rr;
}

}  /* namespace */

extern "C" void xpu_sw_rasterize_triangle(XpuRenderTarget* rt,
                                            const XpuVertex* v0,
                                            const XpuVertex* v1,
                                            const XpuVertex* v2) {
    if (!rt || !v0 || !v1 || !v2) return;

    /* Clip-space w test: skip if all vertices are behind the camera */
    if (v0->position.w < 1e-6f && v1->position.w < 1e-6f && v2->position.w < 1e-6f) {
        return;
    }

    ScreenVertex s0 = to_screen(*v0, rt->width, rt->height);
    ScreenVertex s1 = to_screen(*v1, rt->width, rt->height);
    ScreenVertex s2 = to_screen(*v2, rt->width, rt->height);

    /* Triangle area (signed). Negative = back-facing (we skip culling here). */
    float area = edge_function(s0.x, s0.y, s1.x, s1.y, s2.x, s2.y);
    if (std::fabs(area) < 1e-6f) return;  /* degenerate */
    float inv_area = 1.0f / area;

    /* Bounding box clipped to screen */
    int32_t min_x = (int32_t)std::floor(std::min({s0.x, s1.x, s2.x}));
    int32_t max_x = (int32_t)std::ceil (std::max({s0.x, s1.x, s2.x}));
    int32_t min_y = (int32_t)std::floor(std::min({s0.y, s1.y, s2.y}));
    int32_t max_y = (int32_t)std::ceil (std::max({s0.y, s1.y, s2.y}));
    if (min_x < 0) min_x = 0;
    if (min_y < 0) min_y = 0;
    if (max_x > (int32_t)rt->width  - 1) max_x = (int32_t)rt->width  - 1;
    if (max_y > (int32_t)rt->height - 1) max_y = (int32_t)rt->height - 1;
    if (min_x > max_x || min_y > max_y) return;

    /* Edge function increments for incremental rasterization.
     *
     * Edge function: edge(a, b, p) = (px - ax)*(by - ay) - (py - ay)*(bx - ax)
     *   w0 = edge(s1, s2, p) = (px - s1x)*(s2y - s1y) - (py - s1y)*(s2x - s1x)
     *     d/dx = (s2y - s1y) = dy23
     *     d/dy = -(s2x - s1x) = -dx23
     *   w1 = edge(s2, s0, p): d/dx = dy31, d/dy = -dx31
     *   w2 = edge(s0, s1, p): d/dx = dy12, d/dy = -dx12
     */
    float dx12 = s1.x - s0.x, dy12 = s1.y - s0.y;
    float dx23 = s2.x - s1.x, dy23 = s2.y - s1.y;
    float dx31 = s0.x - s2.x, dy31 = s0.y - s2.y;

    /* Edge function at pixel center (min_x + 0.5, min_y + 0.5) */
    float px = (float)min_x + 0.5f;
    float py = (float)min_y + 0.5f;
    float w0_row = edge_function(s1.x, s1.y, s2.x, s2.y, px, py);
    float w1_row = edge_function(s2.x, s2.y, s0.x, s0.y, px, py);
    float w2_row = edge_function(s0.x, s0.y, s1.x, s1.y, px, py);

    /* Per-step increments in x and y */
    float dw0_dx = dy23;
    float dw0_dy = -dx23;
    float dw1_dx = dy31;
    float dw1_dy = -dx31;
    float dw2_dx = dy12;
    float dw2_dy = -dx12;

    for (int32_t y = min_y; y <= max_y; ++y) {
        float w0 = w0_row;
        float w1 = w1_row;
        float w2 = w2_row;
        uint32_t row_offset = (uint32_t)y * rt->width;
        for (int32_t x = min_x; x <= max_x; ++x) {
            /* All three edges same sign => inside triangle */
            if ((w0 >= 0 && w1 >= 0 && w2 >= 0) || (w0 <= 0 && w1 <= 0 && w2 <= 0)) {
                /* Barycentric coordinates (normalized by area) */
                float b0 = w0 * inv_area;
                float b1 = w1 * inv_area;
                float b2 = w2 * inv_area;
                /* Perspective-correct interpolation:
                 * w_interp = b0*w0_orig + b1*w1_orig + b2*w2_orig
                 * Color = (b0*c0/w0 + b1*c1/w1 + b2*c2/w2) / w_interp
                 * But we pre-divided colors by w, so:
                 * w_interp = b0*inv_w0 + b1*inv_w1 + b2*inv_w2
                 * Color = (b0*r0_div_w + ...) / w_interp */
                float w_interp = b0 * s0.inv_w + b1 * s1.inv_w + b2 * s2.inv_w;
                if (std::fabs(w_interp) < 1e-12f) w_interp = 1e-12f;
                float inv_w_interp = 1.0f / w_interp;
                float r = (b0 * s0.r + b1 * s1.r + b2 * s2.r) * inv_w_interp;
                float g = (b0 * s0.g + b1 * s1.g + b2 * s2.g) * inv_w_interp;
                float b = (b0 * s0.b + b1 * s1.b + b2 * s2.b) * inv_w_interp;
                float a = (b0 * s0.a + b1 * s1.a + b2 * s2.a) * inv_w_interp;
                /* Depth: linear interpolation in NDC z (correct for depth test) */
                float z = b0 * s0.z + b1 * s1.z + b2 * s2.z;
                /* Z-buffer test (map NDC [-1,1] -> [0,1]) */
                float depth_val = z * 0.5f + 0.5f;
                uint32_t pix_idx = row_offset + (uint32_t)x;
                if (depth_val < rt->depth[pix_idx]) {
                    rt->depth[pix_idx] = depth_val;
                    rt->color[pix_idx] = pack_rgba8(r, g, b, a);
                }
            }
            w0 += dw0_dx;
            w1 += dw1_dx;
            w2 += dw2_dx;
        }
        w0_row += dw0_dy;
        w1_row += dw1_dy;
        w2_row += dw2_dy;
    }
}

extern "C" void xpu_sw_rasterize_triangle_list(XpuRenderTarget* rt,
                                                  const XpuVertex* vertices,
                                                  uint32_t vertex_count,
                                                  const uint32_t* indices,
                                                  uint32_t index_count) {
    if (!rt || !vertices || vertex_count == 0) return;
    if (indices && index_count >= 3) {
        for (uint32_t i = 0; i + 2 < index_count; i += 3) {
            uint32_t i0 = indices[i + 0];
            uint32_t i1 = indices[i + 1];
            uint32_t i2 = indices[i + 2];
            if (i0 >= vertex_count || i1 >= vertex_count || i2 >= vertex_count) continue;
            xpu_sw_rasterize_triangle(rt, &vertices[i0], &vertices[i1], &vertices[i2]);
        }
    } else {
        for (uint32_t i = 0; i + 2 < vertex_count; i += 3) {
            xpu_sw_rasterize_triangle(rt, &vertices[i], &vertices[i + 1], &vertices[i + 2]);
        }
    }
}
