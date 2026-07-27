/**
 * XPU - tests/test_rasterizer.cpp
 *
 * Direct test of the software rasterizer - draws a single bright
 * triangle and verifies the output isn't all background.
 */

#include "../src/backend/sw_rasterizer.h"
#include "xpu/xpu_math.h"
#include <cstdio>
#include <cstdlib>

int main() {
    const uint32_t W = 100, H = 100;
    XpuRenderTarget* rt = xpu_sw_rt_create(W, H);
    if (!rt) { std::fprintf(stderr, "rt_create failed\n"); return 1; }

    xpu_sw_rt_clear_color(rt, 0.0f, 0.0f, 0.0f, 1.0f);
    xpu_sw_rt_clear_depth(rt, 1.0f);

    /* Simple triangle in clip space, fully visible */
    XpuVertex v0; v0.position = XpuVec4{-0.5f, -0.5f, 0.0f, 1.0f}; v0.color = XpuVec4{1.0f, 0.0f, 0.0f, 1.0f};
    XpuVertex v1; v1.position = XpuVec4{ 0.5f, -0.5f, 0.0f, 1.0f}; v1.color = XpuVec4{0.0f, 1.0f, 0.0f, 1.0f};
    XpuVertex v2; v2.position = XpuVec4{ 0.0f,  0.5f, 0.0f, 1.0f}; v2.color = XpuVec4{0.0f, 0.0f, 1.0f, 1.0f};

    xpu_sw_rasterize_triangle(rt, &v0, &v1, &v2);

    /* Count non-black pixels */
    uint32_t non_black = 0;
    uint32_t sample_colors[5] = {0};
    int sample_idx = 0;
    for (uint32_t i = 0; i < W * H; ++i) {
        uint32_t p = rt->color[i];
        uint8_t r = p & 0xFF;
        uint8_t g = (p >> 8) & 0xFF;
        uint8_t b = (p >> 16) & 0xFF;
        if (r > 5 || g > 5 || b > 5) {
            ++non_black;
            if (sample_idx < 5) sample_colors[sample_idx++] = p;
        }
    }
    std::printf("Non-black pixels: %u / %u\n", non_black, W * H);
    for (int i = 0; i < sample_idx; ++i) {
        uint32_t p = sample_colors[i];
        std::printf("  sample %d: R=%u G=%u B=%u A=%u\n", i,
                      p & 0xFF, (p >> 8) & 0xFF, (p >> 16) & 0xFF, (p >> 24) & 0xFF);
    }

    xpu_sw_rt_save_bmp(rt, "/tmp/xpu_test_triangle.bmp");
    xpu_sw_rt_destroy(rt);

    if (non_black < 100) {
        std::fprintf(stderr, "FAIL: only %u pixels drawn, expected > 100\n", non_black);
        return 1;
    }
    std::printf("PASS: triangle rasterized successfully\n");
    return 0;
}
