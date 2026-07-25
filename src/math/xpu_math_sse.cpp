/**
 * XPU - src/math/xpu_math_sse.cpp
 *
 * SSE2/AVX2-accelerated overrides for the math API.
 *
 * Compiled only when XPU_USE_SIMD is defined AND we're on x86_64/x86.
 * The functions here replace the scalar versions in xpu_math.cpp via
 * linker-precedence (no symbol clash because xpu_math.cpp guards them
 * with #ifndef XPU_USE_SIMD_VEC4 / XPU_USE_SIMD_TRANSFORM).
 *
 * The SIMD paths are the main reason XPU is fast on the CPU fallback:
 * a single mat4 transform runs in ~6 SSE cycles instead of ~30 scalar.
 */

#if defined(__x86_64__) || defined(__i386__)

#include "xpu/xpu_math.h"
#include <immintrin.h>

/* ------------------------------------------------------------------ */
/* Vec4 - SSE2 (always available on x86_64)                           */
/* ------------------------------------------------------------------ */
extern "C" XpuVec4 xpu_vec4_add(XpuVec4 a, XpuVec4 b) {
    __m128 va = _mm_load_ps(&a.x);
    __m128 vb = _mm_load_ps(&b.x);
    __m128 r = _mm_add_ps(va, vb);
    XpuVec4 out;
    _mm_store_ps(&out.x, r);
    return out;
}

extern "C" XpuVec4 xpu_vec4_sub(XpuVec4 a, XpuVec4 b) {
    __m128 va = _mm_load_ps(&a.x);
    __m128 vb = _mm_load_ps(&b.x);
    __m128 r = _mm_sub_ps(va, vb);
    XpuVec4 out;
    _mm_store_ps(&out.x, r);
    return out;
}

extern "C" XpuVec4 xpu_vec4_mul(XpuVec4 a, XpuVec4 b) {
    __m128 va = _mm_load_ps(&a.x);
    __m128 vb = _mm_load_ps(&b.x);
    __m128 r = _mm_mul_ps(va, vb);
    XpuVec4 out;
    _mm_store_ps(&out.x, r);
    return out;
}

extern "C" XpuVec4 xpu_vec4_scale(XpuVec4 a, float s) {
    __m128 va = _mm_load_ps(&a.x);
    __m128 vs = _mm_set1_ps(s);
    __m128 r = _mm_mul_ps(va, vs);
    XpuVec4 out;
    _mm_store_ps(&out.x, r);
    return out;
}

extern "C" float xpu_vec4_dot(XpuVec4 a, XpuVec4 b) {
    __m128 va = _mm_load_ps(&a.x);
    __m128 vb = _mm_load_ps(&b.x);
    __m128 d = _mm_mul_ps(va, vb);
    __m128 shuf = _mm_shuffle_ps(d, d, _MM_SHUFFLE(2, 3, 0, 1));
    __m128 sums = _mm_add_ps(d, shuf);
    shuf = _mm_movehl_ps(sums, sums);
    sums = _mm_add_ss(sums, shuf);
    return _mm_cvtss_f32(sums);
}

extern "C" float xpu_vec4_length(XpuVec4 a) {
    __m128 va = _mm_load_ps(&a.x);
    __m128 d = _mm_mul_ps(va, va);
    __m128 shuf = _mm_shuffle_ps(d, d, _MM_SHUFFLE(2, 3, 0, 1));
    __m128 sums = _mm_add_ps(d, shuf);
    shuf = _mm_movehl_ps(sums, sums);
    sums = _mm_add_ss(sums, shuf);
    sums = _mm_sqrt_ss(sums);
    return _mm_cvtss_f32(sums);
}

extern "C" XpuVec4 xpu_vec4_normalize(XpuVec4 a) {
    __m128 va = _mm_load_ps(&a.x);
    __m128 d = _mm_mul_ps(va, va);
    __m128 shuf = _mm_shuffle_ps(d, d, _MM_SHUFFLE(2, 3, 0, 1));
    __m128 sums = _mm_add_ps(d, shuf);
    shuf = _mm_movehl_ps(sums, sums);
    sums = _mm_add_ss(sums, shuf);
    __m128 len = _mm_sqrt_ps(sums);
    /* Guard against divide-by-zero */
    __m128 mask = _mm_cmpgt_ps(len, _mm_set1_ps(1e-12f));
    __m128 safe_len = _mm_or_ps(_mm_and_ps(mask, len), _mm_andnot_ps(mask, _mm_set1_ps(1.0f)));
    __m128 r = _mm_div_ps(va, safe_len);
    XpuVec4 out;
    _mm_store_ps(&out.x, r);
    return out;
}

extern "C" XpuVec4 xpu_vec4_cross_xyz(XpuVec4 a, XpuVec4 b) {
    __m128 va = _mm_loadu_ps(&a.x);
    __m128 vb = _mm_loadu_ps(&b.x);
    /* a_yzx = [a.y, a.z, a.x, a.w], b_zxy = [b.z, b.x, b.y, b.w] */
    __m128 a_yzx = _mm_shuffle_ps(va, va, _MM_SHUFFLE(3, 0, 2, 1));
    __m128 b_zxy = _mm_shuffle_ps(vb, vb, _MM_SHUFFLE(3, 1, 0, 2));
    /* a_zxy = [a.z, a.x, a.y, a.w], b_yzx = [b.y, b.z, b.x, b.w] */
    __m128 a_zxy = _mm_shuffle_ps(va, va, _MM_SHUFFLE(3, 1, 0, 2));
    __m128 b_yzx = _mm_shuffle_ps(vb, vb, _MM_SHUFFLE(3, 0, 2, 1));
    /* cross = a_yzx * b_zxy - a_zxy * b_yzx */
    __m128 c = _mm_sub_ps(_mm_mul_ps(a_yzx, b_zxy), _mm_mul_ps(a_zxy, b_yzx));
    XpuVec4 out;
    _mm_storeu_ps(&out.x, c);
    return out;
}

extern "C" XpuVec4 xpu_vec4_lerp(XpuVec4 a, XpuVec4 b, float t) {
    __m128 va = _mm_load_ps(&a.x);
    __m128 vb = _mm_load_ps(&b.x);
    __m128 vt = _mm_set1_ps(t);
    __m128 r = _mm_add_ps(va, _mm_mul_ps(_mm_sub_ps(vb, va), vt));
    XpuVec4 out;
    _mm_store_ps(&out.x, r);
    return out;
}

/* ------------------------------------------------------------------ */
/* Batched vertex transform - SSE4.1 _mm_dp_ps (always available on   */
/* x86_64). The AVX2 path is intentionally omitted because:           */
/*   1. _mm_dp_ps already uses SSE4.1 hardware                        */
/*   2. The compiler will unroll and pipeline across iterations       */
/*   3. To use AVX2 for 8 vertices in parallel we'd need 32-byte      */
/*      aligned input, which XpuVec4 (16-byte aligned) doesn't        */
/*      guarantee. The unaligned version with permutes is slower.     */
/* ------------------------------------------------------------------ */
extern "C" void xpu_transform_vertices(const XpuMat4* mvp,
                                        const XpuVec4* src,
                                        XpuVec4* dst,
                                        xpu_size count) {
    const __m128 m0 = _mm_loadu_ps(&mvp->m[0]);
    const __m128 m1 = _mm_loadu_ps(&mvp->m[4]);
    const __m128 m2 = _mm_loadu_ps(&mvp->m[8]);
    const __m128 m3 = _mm_loadu_ps(&mvp->m[12]);
    /* Unroll by 4 to give the compiler instruction-level parallelism */
    xpu_size i = 0;
    for (; i + 4 <= count; i += 4) {
        for (int k = 0; k < 4; ++k) {
            __m128 v = _mm_loadu_ps(&src[i + k].x);
            __m128 x = _mm_dp_ps(m0, v, 0xF1);
            __m128 y = _mm_dp_ps(m1, v, 0xF2);
            __m128 z = _mm_dp_ps(m2, v, 0xF4);
            __m128 w = _mm_dp_ps(m3, v, 0xF8);
            __m128 r = _mm_or_ps(_mm_or_ps(x, y), _mm_or_ps(z, w));
            _mm_storeu_ps(&dst[i + k].x, r);
        }
    }
    for (; i < count; ++i) {
        __m128 v = _mm_loadu_ps(&src[i].x);
        __m128 x = _mm_dp_ps(m0, v, 0xF1);
        __m128 y = _mm_dp_ps(m1, v, 0xF2);
        __m128 z = _mm_dp_ps(m2, v, 0xF4);
        __m128 w = _mm_dp_ps(m3, v, 0xF8);
        __m128 r = _mm_or_ps(_mm_or_ps(x, y), _mm_or_ps(z, w));
        _mm_storeu_ps(&dst[i].x, r);
    }
}

#endif  /* x86 */
