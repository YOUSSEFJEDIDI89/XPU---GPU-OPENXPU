/**
 * XPU - src/math/xpu_math.cpp
 *
 * Scalar reference implementation of the math API + CPU detection.
 * SIMD-optimized overrides live in xpu_math_sse.cpp and xpu_math_neon.cpp;
 * the linker picks whichever matches the target architecture.
 *
 * CPU detection uses CPUID on x86 and /proc/cpuinfo on ARM (Linux/Android).
 */

#include "xpu/xpu_math.h"
#include <cstring>
#include <cmath>
#include <cstdio>

#if defined(__x86_64__) || defined(__i386__)
  #include <cpuid.h>
  #define XPU_HAS_CPUID 1
#elif defined(__linux__) && (defined(__arm__) || defined(__aarch64__))
  #include <fstream>
  #include <sstream>
  #include <string>
#endif

/* ------------------------------------------------------------------ */
/* CPU architecture detection                                          */
/* ------------------------------------------------------------------ */
extern "C" XpuCpuArch xpu_math_detect_cpu_arch(void) {
#if defined(XPU_HAS_CPUID)
    unsigned int eax, ebx, ecx, edx;
    if (__builtin_cpu_supports("avx512f")) return XPU_CPU_ARCH_X86_AVX512;
    if (__builtin_cpu_supports("avx2"))    return XPU_CPU_ARCH_X86_AVX2;
    if (__builtin_cpu_supports("sse2"))    return XPU_CPU_ARCH_X86_SSE2;
    return XPU_CPU_ARCH_X86_SSE2;  /* x86_64 always has SSE2 */
#elif defined(__aarch64__)
    return XPU_CPU_ARCH_ARM_NEON64;  /* AArch64 always has NEON */
#elif defined(__ARM_NEON)
    return XPU_CPU_ARCH_ARM_NEON;
#elif defined(__arm__)
    /* Old ARMv7 without NEON - e.g. very old MediaTek MT6572 */
    return XPU_CPU_ARCH_ARM_VFP;
#else
    return XPU_CPU_ARCH_UNKNOWN;
#endif
}

extern "C" const char* xpu_math_arch_name(XpuCpuArch a) {
    switch (a) {
        case XPU_CPU_ARCH_X86_SSE2:    return "x86 SSE2";
        case XPU_CPU_ARCH_X86_AVX2:    return "x86 AVX2";
        case XPU_CPU_ARCH_X86_AVX512:  return "x86 AVX-512";
        case XPU_CPU_ARCH_ARM_NEON:    return "ARM NEON";
        case XPU_CPU_ARCH_ARM_NEON64:  return "AArch64 NEON";
        case XPU_CPU_ARCH_ARM_VFP:     return "ARM VFP (legacy)";
        case XPU_CPU_ARCH_PPC:         return "PowerPC";
        case XPU_CPU_ARCH_RISCV:       return "RISC-V";
        default:                       return "unknown";
    }
}

/* ------------------------------------------------------------------ */
/* Scalar Vec4 math - used as fallback if no SIMD override is linked  */
/* ------------------------------------------------------------------ */
#ifndef XPU_USE_SIMD_VEC4

extern "C" XpuVec4 xpu_vec4_add(XpuVec4 a, XpuVec4 b) {
    return XpuVec4{a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w};
}

extern "C" XpuVec4 xpu_vec4_sub(XpuVec4 a, XpuVec4 b) {
    return XpuVec4{a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w};
}

extern "C" XpuVec4 xpu_vec4_mul(XpuVec4 a, XpuVec4 b) {
    return XpuVec4{a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w};
}

extern "C" XpuVec4 xpu_vec4_scale(XpuVec4 a, float s) {
    return XpuVec4{a.x * s, a.y * s, a.z * s, a.w * s};
}

extern "C" float xpu_vec4_dot(XpuVec4 a, XpuVec4 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

extern "C" float xpu_vec4_length(XpuVec4 a) {
    return std::sqrt(xpu_vec4_dot(a, a));
}

extern "C" XpuVec4 xpu_vec4_normalize(XpuVec4 a) {
    float l = xpu_vec4_length(a);
    if (l < 1e-12f) return XpuVec4{0, 0, 0, 0};
    float inv = 1.0f / l;
    return xpu_vec4_scale(a, inv);
}

extern "C" XpuVec4 xpu_vec4_cross_xyz(XpuVec4 a, XpuVec4 b) {
    return XpuVec4{
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
        0.0f
    };
}

extern "C" XpuVec4 xpu_vec4_lerp(XpuVec4 a, XpuVec4 b, float t) {
    return XpuVec4{
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t,
        a.w + (b.w - a.w) * t
    };
}

#endif /* XPU_USE_SIMD_VEC4 */

/* ------------------------------------------------------------------ */
/* Matrix 4x4                                                          */
/* ------------------------------------------------------------------ */
extern "C" XpuMat4 xpu_mat4_identity(void) {
    XpuMat4 r;
    std::memset(&r, 0, sizeof(r));
    r.m[0]  = 1.0f; r.m[5]  = 1.0f;
    r.m[10] = 1.0f; r.m[15] = 1.0f;
    return r;
}

extern "C" XpuMat4 xpu_mat4_mul(XpuMat4 a, XpuMat4 b) {
    XpuMat4 r;
    /* row-major: r[i][j] = sum_k a[i][k] * b[k][j] */
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            float s = 0.0f;
            for (int k = 0; k < 4; ++k) {
                s += a.m[i * 4 + k] * b.m[k * 4 + j];
            }
            r.m[i * 4 + j] = s;
        }
    }
    return r;
}

extern "C" XpuMat4 xpu_mat4_transpose(XpuMat4 m) {
    XpuMat4 r;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            r.m[i * 4 + j] = m.m[j * 4 + i];
    return r;
}

extern "C" XpuVec4 xpu_mat4_transform(XpuMat4 m, XpuVec4 v) {
    XpuVec4 r;
    r.x = m.m[0]  * v.x + m.m[1]  * v.y + m.m[2]  * v.z + m.m[3]  * v.w;
    r.y = m.m[4]  * v.x + m.m[5]  * v.y + m.m[6]  * v.z + m.m[7]  * v.w;
    r.z = m.m[8]  * v.x + m.m[9]  * v.y + m.m[10] * v.z + m.m[11] * v.w;
    r.w = m.m[12] * v.x + m.m[13] * v.y + m.m[14] * v.z + m.m[15] * v.w;
    return r;
}

extern "C" XpuMat4 xpu_mat4_inverse(XpuMat4 m) {
    /* General 4x4 inverse using cofactors. Slow but correct. */
    XpuMat4 r;
    const float* a = m.m;
    float inv[16];

    inv[0] = a[5]*a[10]*a[15] - a[5]*a[11]*a[14] - a[9]*a[6]*a[15] + a[9]*a[7]*a[14] + a[13]*a[6]*a[11] - a[13]*a[7]*a[10];
    inv[4] = -a[4]*a[10]*a[15] + a[4]*a[11]*a[14] + a[8]*a[6]*a[15] - a[8]*a[7]*a[14] - a[12]*a[6]*a[11] + a[12]*a[7]*a[10];
    inv[8] = a[4]*a[9]*a[15] - a[4]*a[11]*a[13] - a[8]*a[5]*a[15] + a[8]*a[7]*a[13] + a[12]*a[5]*a[11] - a[12]*a[7]*a[9];
    inv[12] = -a[4]*a[9]*a[14] + a[4]*a[10]*a[13] + a[8]*a[5]*a[14] - a[8]*a[6]*a[13] - a[12]*a[5]*a[10] + a[12]*a[6]*a[9];

    float det = a[0]*inv[0] + a[1]*inv[4] + a[2]*inv[8] + a[3]*inv[12];
    if (std::fabs(det) < 1e-20f) {
        return xpu_mat4_identity();
    }
    float inv_det = 1.0f / det;

    inv[1]  = -a[1]*a[10]*a[15] + a[1]*a[11]*a[14] + a[9]*a[2]*a[15] - a[9]*a[3]*a[14] - a[13]*a[2]*a[11] + a[13]*a[3]*a[10];
    inv[5]  = a[0]*a[10]*a[15] - a[0]*a[11]*a[14] - a[8]*a[2]*a[15] + a[8]*a[3]*a[14] + a[12]*a[2]*a[11] - a[12]*a[3]*a[10];
    inv[9]  = -a[0]*a[9]*a[15] + a[0]*a[11]*a[13] + a[8]*a[1]*a[15] - a[8]*a[3]*a[13] - a[12]*a[1]*a[11] + a[12]*a[3]*a[9];
    inv[13] = a[0]*a[9]*a[14] - a[0]*a[10]*a[13] - a[8]*a[1]*a[14] + a[8]*a[2]*a[13] + a[12]*a[1]*a[10] - a[12]*a[2]*a[9];

    inv[2]  = a[1]*a[6]*a[15] - a[1]*a[7]*a[14] - a[5]*a[2]*a[15] + a[5]*a[3]*a[14] + a[13]*a[2]*a[7] - a[13]*a[3]*a[6];
    inv[6]  = -a[0]*a[6]*a[15] + a[0]*a[7]*a[14] + a[4]*a[2]*a[15] - a[4]*a[3]*a[14] - a[12]*a[2]*a[7] + a[12]*a[3]*a[6];
    inv[10] = a[0]*a[5]*a[15] - a[0]*a[7]*a[13] - a[4]*a[1]*a[15] + a[4]*a[3]*a[13] + a[12]*a[1]*a[7] - a[12]*a[3]*a[5];
    inv[14] = -a[0]*a[5]*a[14] + a[0]*a[6]*a[13] + a[4]*a[1]*a[14] - a[4]*a[2]*a[13] - a[12]*a[1]*a[6] + a[12]*a[2]*a[5];

    inv[3]  = -a[1]*a[6]*a[11] + a[1]*a[7]*a[10] + a[5]*a[2]*a[11] - a[5]*a[3]*a[10] - a[9]*a[2]*a[7] + a[9]*a[3]*a[6];
    inv[7]  = a[0]*a[6]*a[11] - a[0]*a[7]*a[10] - a[4]*a[2]*a[11] + a[4]*a[3]*a[10] + a[8]*a[2]*a[7] - a[8]*a[3]*a[6];
    inv[11] = -a[0]*a[5]*a[11] + a[0]*a[7]*a[9] + a[4]*a[1]*a[11] - a[4]*a[3]*a[9] - a[8]*a[1]*a[7] + a[8]*a[3]*a[5];
    inv[15] = a[0]*a[5]*a[10] - a[0]*a[6]*a[9] - a[4]*a[1]*a[10] + a[4]*a[2]*a[9] + a[8]*a[1]*a[6] - a[8]*a[2]*a[5];

    for (int i = 0; i < 16; ++i) r.m[i] = inv[i] * inv_det;
    return r;
}

extern "C" XpuMat4 xpu_mat4_translate(float x, float y, float z) {
    XpuMat4 r = xpu_mat4_identity();
    r.m[3]  = x;
    r.m[7]  = y;
    r.m[11] = z;
    return r;
}

extern "C" XpuMat4 xpu_mat4_scale(float x, float y, float z) {
    XpuMat4 r = xpu_mat4_identity();
    r.m[0]  = x;
    r.m[5]  = y;
    r.m[10] = z;
    return r;
}

extern "C" XpuMat4 xpu_mat4_rotate_x(float rad) {
    XpuMat4 r = xpu_mat4_identity();
    float c = std::cos(rad), s = std::sin(rad);
    r.m[5] = c;  r.m[6] = -s;
    r.m[9] = s;  r.m[10] = c;
    return r;
}

extern "C" XpuMat4 xpu_mat4_rotate_y(float rad) {
    XpuMat4 r = xpu_mat4_identity();
    float c = std::cos(rad), s = std::sin(rad);
    r.m[0] = c;   r.m[2] = s;
    r.m[8] = -s;  r.m[10] = c;
    return r;
}

extern "C" XpuMat4 xpu_mat4_rotate_z(float rad) {
    XpuMat4 r = xpu_mat4_identity();
    float c = std::cos(rad), s = std::sin(rad);
    r.m[0] = c;  r.m[1] = -s;
    r.m[4] = s;  r.m[5] = c;
    return r;
}

extern "C" XpuMat4 xpu_mat4_rotate_axis(XpuVec3 axis, float rad) {
    /* Rodrigues formula */
    XpuVec3 n{ axis.x, axis.y, axis.z };
    float l = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
    if (l > 1e-12f) { n.x /= l; n.y /= l; n.z /= l; }
    float c = std::cos(rad), s = std::sin(rad), t = 1.0f - c;
    XpuMat4 r = xpu_mat4_identity();
    r.m[0] = t * n.x * n.x + c;
    r.m[1] = t * n.x * n.y - s * n.z;
    r.m[2] = t * n.x * n.z + s * n.y;
    r.m[4] = t * n.x * n.y + s * n.z;
    r.m[5] = t * n.y * n.y + c;
    r.m[6] = t * n.y * n.z - s * n.x;
    r.m[8] = t * n.x * n.z - s * n.y;
    r.m[9] = t * n.y * n.z + s * n.x;
    r.m[10] = t * n.z * n.z + c;
    return r;
}

extern "C" XpuMat4 xpu_mat4_lookat(XpuVec3 eye, XpuVec3 center, XpuVec3 up) {
    XpuVec3 f{ center.x - eye.x, center.y - eye.y, center.z - eye.z };
    float fl = std::sqrt(f.x * f.x + f.y * f.y + f.z * f.z);
    f.x /= fl; f.y /= fl; f.z /= fl;
    /* s = f x up */
    XpuVec3 s{
        f.y * up.z - f.z * up.y,
        f.z * up.x - f.x * up.z,
        f.x * up.y - f.y * up.x
    };
    float sl = std::sqrt(s.x * s.x + s.y * s.y + s.z * s.z);
    s.x /= sl; s.y /= sl; s.z /= sl;
    /* u = s x f */
    XpuVec3 u{
        s.y * f.z - s.z * f.y,
        s.z * f.x - s.x * f.z,
        s.x * f.y - s.y * f.x
    };
    XpuMat4 r = xpu_mat4_identity();
    r.m[0] = s.x;  r.m[1] = s.y;  r.m[2] = s.z;  r.m[3]  = -(s.x * eye.x + s.y * eye.y + s.z * eye.z);
    r.m[4] = u.x;  r.m[5] = u.y;  r.m[6] = u.z;  r.m[7]  = -(u.x * eye.x + u.y * eye.y + u.z * eye.z);
    r.m[8] = -f.x; r.m[9] = -f.y; r.m[10] = -f.z; r.m[11] = (f.x * eye.x + f.y * eye.y + f.z * eye.z);
    return r;
}

extern "C" XpuMat4 xpu_mat4_perspective(float fovy, float aspect, float near, float far) {
    XpuMat4 r;
    std::memset(&r, 0, sizeof(r));
    float f = 1.0f / std::tan(fovy * 0.5f);
    r.m[0]  = f / aspect;
    r.m[5]  = f;
    r.m[10] = (far + near) / (near - far);
    r.m[11] = (2.0f * far * near) / (near - far);
    r.m[14] = -1.0f;
    return r;
}

extern "C" XpuMat4 xpu_mat4_orthographic(float l, float r_, float b, float t, float n, float f) {
    XpuMat4 r = xpu_mat4_identity();
    r.m[0]  = 2.0f / (r_ - l);
    r.m[5]  = 2.0f / (t - b);
    r.m[10] = -2.0f / (f - n);
    r.m[3]  = -(r_ + l) / (r_ - l);
    r.m[7]  = -(t + b) / (t - b);
    r.m[11] = -(f + n) / (f - n);
    return r;
}

/* ------------------------------------------------------------------ */
/* Quaternions                                                         */
/* ------------------------------------------------------------------ */
extern "C" XpuQuat xpu_quat_identity(void) {
    return XpuQuat{0.0f, 0.0f, 0.0f, 1.0f};
}

extern "C" XpuQuat xpu_quat_from_axis_angle(XpuVec3 axis, float rad) {
    float l = std::sqrt(axis.x * axis.x + axis.y * axis.y + axis.z * axis.z);
    if (l < 1e-12f) return xpu_quat_identity();
    float s = std::sin(rad * 0.5f) / l;
    return XpuQuat{axis.x * s, axis.y * s, axis.z * s, std::cos(rad * 0.5f)};
}

extern "C" XpuQuat xpu_quat_mul(XpuQuat a, XpuQuat b) {
    return XpuQuat{
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z
    };
}

extern "C" XpuQuat xpu_quat_normalize(XpuQuat q) {
    float l = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (l < 1e-12f) return xpu_quat_identity();
    float inv = 1.0f / l;
    return XpuQuat{q.x * inv, q.y * inv, q.z * inv, q.w * inv};
}

extern "C" XpuMat4 xpu_quat_to_mat4(XpuQuat q) {
    XpuQuat n = xpu_quat_normalize(q);
    float xx = n.x * n.x, yy = n.y * n.y, zz = n.z * n.z;
    float xy = n.x * n.y, xz = n.x * n.z, yz = n.y * n.z;
    float wx = n.w * n.x, wy = n.w * n.y, wz = n.w * n.z;
    XpuMat4 r = xpu_mat4_identity();
    r.m[0]  = 1.0f - 2.0f * (yy + zz);
    r.m[1]  = 2.0f * (xy - wz);
    r.m[2]  = 2.0f * (xz + wy);
    r.m[4]  = 2.0f * (xy + wz);
    r.m[5]  = 1.0f - 2.0f * (xx + zz);
    r.m[6]  = 2.0f * (yz - wx);
    r.m[8]  = 2.0f * (xz - wy);
    r.m[9]  = 2.0f * (yz + wx);
    r.m[10] = 1.0f - 2.0f * (xx + yy);
    return r;
}

/* ------------------------------------------------------------------ */
/* Batched vertex transform - scalar fallback                          */
/* ------------------------------------------------------------------ */
#ifndef XPU_USE_SIMD_TRANSFORM
extern "C" void xpu_transform_vertices(const XpuMat4* mvp,
                                        const XpuVec4* src,
                                        XpuVec4* dst,
                                        xpu_size count) {
    /* Unrolled by 4 to give the autovectorizer something to chew on */
    xpu_size i = 0;
    for (; i + 4 <= count; i += 4) {
        dst[i + 0] = xpu_mat4_transform(*mvp, src[i + 0]);
        dst[i + 1] = xpu_mat4_transform(*mvp, src[i + 1]);
        dst[i + 2] = xpu_mat4_transform(*mvp, src[i + 2]);
        dst[i + 3] = xpu_mat4_transform(*mvp, src[i + 3]);
    }
    for (; i < count; ++i) {
        dst[i] = xpu_mat4_transform(*mvp, src[i]);
    }
}
#endif
