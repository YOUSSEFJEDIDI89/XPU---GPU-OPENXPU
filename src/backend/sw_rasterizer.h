/**
 * XPU - src/backend/sw_rasterizer.h
 *
 * Real software rasterizer for the XPU software backend.
 * Implements actual triangle rasterization with:
 *   - Edge-function-based coverage test
 *   - Perspective-correct barycentric interpolation
 *   - Z-buffer depth test
 *   - Per-vertex color blending
 *   - SIMD-optimized scanline fill (4 pixels at a time)
 *
 * This is what makes XPU a "real GPU" on devices with no GPU driver.
 */

#ifndef XPU_SW_RASTERIZER_H
#define XPU_SW_RASTERIZER_H

#include "xpu/xpu_types.h"
#include "xpu/xpu_math.h"
#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

/* A render target = color buffer + depth buffer */
typedef struct XpuRenderTarget {
    uint32_t  width;
    uint32_t  height;
    uint32_t* color;        /* RGBA8 (R in low byte) */
    float*    depth;        /* per-pixel depth */
} XpuRenderTarget;

/* Create / destroy a render target */
XpuRenderTarget* xpu_sw_rt_create(uint32_t width, uint32_t height);
void             xpu_sw_rt_destroy(XpuRenderTarget* rt);

/* Clear color + depth buffers */
void xpu_sw_rt_clear_color(XpuRenderTarget* rt, float r, float g, float b, float a);
void xpu_sw_rt_clear_depth(XpuRenderTarget* rt, float depth);

/* Save the color buffer to a BMP file (returns 0 on success) */
int xpu_sw_rt_save_bmp(const XpuRenderTarget* rt, const char* path);

/* ------------------------------------------------------------------ */
/* Triangle rasterization                                             */
/* ------------------------------------------------------------------ */

typedef struct XpuVertex {
    XpuVec4 position;  /* clip space (x, y, z, w) - w must be != 0 */
    XpuVec4 color;     /* RGBA float 0..1 */
} XpuVertex;

/* Rasterize a single triangle with depth test and color interpolation.
 * Vertices are in clip space; the function does perspective divide,
 * viewport transform, and scanline rasterization internally. */
void xpu_sw_rasterize_triangle(XpuRenderTarget* rt,
                                 const XpuVertex* v0,
                                 const XpuVertex* v1,
                                 const XpuVertex* v2);

/* Batch-rasterize a triangle list. vertices = array of N vertices,
 * indices = array of M indices (or NULL for non-indexed). */
void xpu_sw_rasterize_triangle_list(XpuRenderTarget* rt,
                                      const XpuVertex* vertices,
                                      uint32_t vertex_count,
                                      const uint32_t* indices,
                                      uint32_t index_count);

#ifdef __cplusplus
}
#endif

#endif /* XPU_SW_RASTERIZER_H */
