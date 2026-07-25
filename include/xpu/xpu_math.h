/**
 * XPU - xpu_math.h - SIMD-accelerated vector/matrix math library
 *
 * This is the heart of the XPU software fallback. When no real GPU is
 * available (or for old phones without proper drivers), XPU falls back
 * to CPU rasterization using these math routines, which are vectorized
 * using SSE2/AVX2 on x86 and NEON on ARM.
 *
 * The functions are declared `extern "C"` so they can be called from
 * JNI and P/Invoke.
 */

#ifndef XPU_MATH_H
#define XPU_MATH_H

#include "xpu_types.h"

/* Make sure XPU_API is visible even when this header is included alone */
#ifndef XPU_API
  #include "xpu.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Vector types - 16-byte aligned for SIMD                            */
/* ------------------------------------------------------------------ */
typedef struct __attribute__((aligned(16))) XpuVec2 { float x, y; } XpuVec2;
typedef struct __attribute__((aligned(16))) XpuVec3 { float x, y, z; } XpuVec3;
typedef struct __attribute__((aligned(16))) XpuVec4 { float x, y, z, w; } XpuVec4;
typedef struct __attribute__((aligned(16))) XpuMat4 { float m[16]; } XpuMat4;     /* row-major */
typedef struct __attribute__((aligned(16))) XpuQuat { float x, y, z, w; } XpuQuat;

/* ------------------------------------------------------------------ */
/* Vec4 arithmetic - SIMD-vectorized (4 floats per op)                */
/* ------------------------------------------------------------------ */
XPU_API XpuVec4 xpu_vec4_add(XpuVec4 a, XpuVec4 b);
XPU_API XpuVec4 xpu_vec4_sub(XpuVec4 a, XpuVec4 b);
XPU_API XpuVec4 xpu_vec4_mul(XpuVec4 a, XpuVec4 b);
XPU_API XpuVec4 xpu_vec4_scale(XpuVec4 a, float s);
XPU_API float   xpu_vec4_dot(XpuVec4 a, XpuVec4 b);
XPU_API float   xpu_vec4_length(XpuVec4 a);
XPU_API XpuVec4 xpu_vec4_normalize(XpuVec4 a);
XPU_API XpuVec4 xpu_vec4_cross_xyz(XpuVec4 a, XpuVec4 b);  /* cross of xyz, w=0 */
XPU_API XpuVec4 xpu_vec4_lerp(XpuVec4 a, XpuVec4 b, float t);

/* ------------------------------------------------------------------ */
/* Matrix 4x4 - column-major math like OpenGL                         */
/* ------------------------------------------------------------------ */
XPU_API XpuMat4 xpu_mat4_identity(void);
XPU_API XpuMat4 xpu_mat4_mul(XpuMat4 a, XpuMat4 b);
XPU_API XpuMat4 xpu_mat4_transpose(XpuMat4 m);
XPU_API XpuMat4 xpu_mat4_inverse(XpuMat4 m);
XPU_API XpuVec4 xpu_mat4_transform(XpuMat4 m, XpuVec4 v);

/* Common transforms */
XPU_API XpuMat4 xpu_mat4_translate(float x, float y, float z);
XPU_API XpuMat4 xpu_mat4_scale(float x, float y, float z);
XPU_API XpuMat4 xpu_mat4_rotate_x(float radians);
XPU_API XpuMat4 xpu_mat4_rotate_y(float radians);
XPU_API XpuMat4 xpu_mat4_rotate_z(float radians);
XPU_API XpuMat4 xpu_mat4_rotate_axis(XpuVec3 axis, float radians);

/* View / projection */
XPU_API XpuMat4 xpu_mat4_lookat(XpuVec3 eye, XpuVec3 center, XpuVec3 up);
XPU_API XpuMat4 xpu_mat4_perspective(float fovy_radians, float aspect, float near, float far);
XPU_API XpuMat4 xpu_mat4_orthographic(float left, float right, float bottom, float top, float near, float far);

/* ------------------------------------------------------------------ */
/* Quaternion                                                          */
/* ------------------------------------------------------------------ */
XPU_API XpuQuat xpu_quat_identity(void);
XPU_API XpuQuat xpu_quat_from_axis_angle(XpuVec3 axis, float radians);
XPU_API XpuQuat xpu_quat_mul(XpuQuat a, XpuQuat b);
XPU_API XpuQuat xpu_quat_normalize(XpuQuat q);
XPU_API XpuMat4 xpu_quat_to_mat4(XpuQuat q);

/* ------------------------------------------------------------------ */
/* CPU capability detection - drives which SIMD path is used          */
/* ------------------------------------------------------------------ */
XPU_API XpuCpuArch xpu_math_detect_cpu_arch(void);
XPU_API const char* xpu_math_arch_name(XpuCpuArch arch);

/* ------------------------------------------------------------------ */
/* Batched vertex transform - for software rasterizer               */
/* Processes N vertices in parallel using SIMD                       */
/* ------------------------------------------------------------------ */
XPU_API void xpu_transform_vertices(const XpuMat4* mvp,
                                      const XpuVec4* src,
                                      XpuVec4* dst,
                                      xpu_size count);

#ifdef __cplusplus
}
#endif

#endif /* XPU_MATH_H */
