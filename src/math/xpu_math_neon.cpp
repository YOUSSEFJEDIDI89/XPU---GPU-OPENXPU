/**
 * XPU - src/math/xpu_math_neon.cpp
 *
 * NEON-accelerated overrides for ARM (both ARMv7-A with NEON and AArch64).
 * These are the paths that run on Snapdragon, MediaTek, and Apple Silicon.
 *
 * COMPATIBILITY: Uses only standard NEON intrinsics - no GCC-only builtins.
 * Tested with gcc, clang (Android NDK), and Apple clang.
 */

#if defined(__ARM_NEON) || defined(__aarch64__)

#include "xpu/xpu_math.h"
#include <arm_neon.h>
#include <cmath>   /* for sqrtf - needed by clang / Android NDK */

extern "C" XpuVec4 xpu_vec4_add(XpuVec4 a, XpuVec4 b) {
    float32x4_t va = vld1q_f32(&a.x);
    float32x4_t vb = vld1q_f32(&b.x);
    float32x4_t r = vaddq_f32(va, vb);
    XpuVec4 out;
    vst1q_f32(&out.x, r);
    return out;
}

extern "C" XpuVec4 xpu_vec4_sub(XpuVec4 a, XpuVec4 b) {
    float32x4_t va = vld1q_f32(&a.x);
    float32x4_t vb = vld1q_f32(&b.x);
    float32x4_t r = vsubq_f32(va, vb);
    XpuVec4 out;
    vst1q_f32(&out.x, r);
    return out;
}

extern "C" XpuVec4 xpu_vec4_mul(XpuVec4 a, XpuVec4 b) {
    float32x4_t va = vld1q_f32(&a.x);
    float32x4_t vb = vld1q_f32(&b.x);
    float32x4_t r = vmulq_f32(va, vb);
    XpuVec4 out;
    vst1q_f32(&out.x, r);
    return out;
}

extern "C" XpuVec4 xpu_vec4_scale(XpuVec4 a, float s) {
    float32x4_t va = vld1q_f32(&a.x);
    float32x4_t r = vmulq_n_f32(va, s);
    XpuVec4 out;
    vst1q_f32(&out.x, r);
    return out;
}

extern "C" float xpu_vec4_dot(XpuVec4 a, XpuVec4 b) {
    float32x4_t va = vld1q_f32(&a.x);
    float32x4_t vb = vld1q_f32(&b.x);
    float32x4_t p = vmulq_f32(va, vb);
#if defined(__aarch64__)
    return vaddvq_f32(p);
#else
    float32x2_t s = vpadd_f32(vget_low_f32(p), vget_high_f32(p));
    s = vpadd_f32(s, s);
    return vget_lane_f32(s, 0);
#endif
}

extern "C" float xpu_vec4_length(XpuVec4 a) {
    float32x4_t va = vld1q_f32(&a.x);
    float32x4_t p = vmulq_f32(va, va);
#if defined(__aarch64__)
    return std::sqrt(vaddvq_f32(p));
#else
    float32x2_t s = vpadd_f32(vget_low_f32(p), vget_high_f32(p));
    s = vpadd_f32(s, s);
    return std::sqrt(vget_lane_f32(s, 0));
#endif
}

extern "C" XpuVec4 xpu_vec4_normalize(XpuVec4 a) {
    float32x4_t va = vld1q_f32(&a.x);
    float32x4_t p = vmulq_f32(va, va);
#if defined(__aarch64__)
    float len = std::sqrt(vaddvq_f32(p));
#else
    float32x2_t s = vpadd_f32(vget_low_f32(p), vget_high_f32(p));
    s = vpadd_f32(s, s);
    float len = std::sqrt(vget_lane_f32(s, 0));
#endif
    float32x4_t r;
    if (len > 1e-12f) {
        r = vmulq_n_f32(va, 1.0f / len);
    } else {
        r = vdupq_n_f32(0.0f);
    }
    XpuVec4 out;
    vst1q_f32(&out.x, r);
    return out;
}

/* Cross product using lane extraction (portable across gcc/clang) */
extern "C" XpuVec4 xpu_vec4_cross_xyz(XpuVec4 a, XpuVec4 b) {
    float32x4_t va = vld1q_f32(&a.x);
    float32x4_t vb = vld1q_f32(&b.x);
    /* Extract components */
    float ax = vgetq_lane_f32(va, 0);
    float ay = vgetq_lane_f32(va, 1);
    float az = vgetq_lane_f32(va, 2);
    float bx = vgetq_lane_f32(vb, 0);
    float by = vgetq_lane_f32(vb, 1);
    float bz = vgetq_lane_f32(vb, 2);
    /* cross = (ay*bz - az*by, az*bx - ax*bz, ax*by - ay*bx, 0) */
    float32x4_t r;
    r = vsetq_lane_f32(ay * bz - az * by, r, 0);
    r = vsetq_lane_f32(az * bx - ax * bz, r, 1);
    r = vsetq_lane_f32(ax * by - ay * bx, r, 2);
    r = vsetq_lane_f32(0.0f, r, 3);
    XpuVec4 out;
    vst1q_f32(&out.x, r);
    return out;
}

extern "C" XpuVec4 xpu_vec4_lerp(XpuVec4 a, XpuVec4 b, float t) {
    float32x4_t va = vld1q_f32(&a.x);
    float32x4_t vb = vld1q_f32(&b.x);
    float32x4_t r = vmlaq_n_f32(va, vsubq_f32(vb, va), t);
    XpuVec4 out;
    vst1q_f32(&out.x, r);
    return out;
}

/* ------------------------------------------------------------------ */
/* Batched vertex transform - NEON                                   */
/* ------------------------------------------------------------------ */
extern "C" void xpu_transform_vertices(const XpuMat4* mvp,
                                        const XpuVec4* src,
                                        XpuVec4* dst,
                                        xpu_size count) {
    const float32x4_t m0 = vld1q_f32(&mvp->m[0]);
    const float32x4_t m1 = vld1q_f32(&mvp->m[4]);
    const float32x4_t m2 = vld1q_f32(&mvp->m[8]);
    const float32x4_t m3 = vld1q_f32(&mvp->m[12]);

    for (xpu_size i = 0; i < count; ++i) {
        float32x4_t vv = vld1q_f32(&src[i].x);
        /* Per-component: dot product of (m_row, v) */
        float32x4_t px = vmulq_f32(m0, vv);
        float32x4_t py = vmulq_f32(m1, vv);
        float32x4_t pz = vmulq_f32(m2, vv);
        float32x4_t pw = vmulq_f32(m3, vv);
        /* Horizontal add each */
#if defined(__aarch64__)
        float x = vaddvq_f32(px);
        float y = vaddvq_f32(py);
        float z = vaddvq_f32(pz);
        float w = vaddvq_f32(pw);
#else
        float32x2_t sx = vpadd_f32(vget_low_f32(px), vget_high_f32(px));
        sx = vpadd_f32(sx, sx);
        float x = vget_lane_f32(sx, 0);
        float32x2_t sy = vpadd_f32(vget_low_f32(py), vget_high_f32(py));
        sy = vpadd_f32(sy, sy);
        float y = vget_lane_f32(sy, 0);
        float32x2_t sz = vpadd_f32(vget_low_f32(pz), vget_high_f32(pz));
        sz = vpadd_f32(sz, sz);
        float z = vget_lane_f32(sz, 0);
        float32x2_t sw = vpadd_f32(vget_low_f32(pw), vget_high_f32(pw));
        sw = vpadd_f32(sw, sw);
        float w = vget_lane_f32(sw, 0);
#endif
        float32x4_t r = {x, y, z, w};
        vst1q_f32(&dst[i].x, r);
    }
}

#endif /* __ARM_NEON || __aarch64__ */
