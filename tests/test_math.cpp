/**
 * XPU - tests/test_math.cpp
 *
 * Sanity tests for the XPU SIMD math library.
 */

#include "xpu/xpu_math.h"
#include <cstdio>
#include <cmath>
#include <cstdlib>

static int g_failures = 0;

static void check(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++g_failures;
    } else {
        std::printf("ok   : %s\n", msg);
    }
}

static bool vec4_eq(XpuVec4 a, XpuVec4 b, float eps = 1e-5f) {
    return std::fabs(a.x - b.x) < eps &&
           std::fabs(a.y - b.y) < eps &&
           std::fabs(a.z - b.z) < eps &&
           std::fabs(a.w - b.w) < eps;
}

int main() {
    std::printf("=== XPU Math Tests ===\n");
    std::printf("CPU arch: %s\n", xpu_math_arch_name(xpu_math_detect_cpu_arch()));

    XpuVec4 a{1, 2, 3, 4};
    XpuVec4 b{10, 20, 30, 40};
    check(vec4_eq(xpu_vec4_add(a, b), {11, 22, 33, 44}), "vec4_add");
    check(vec4_eq(xpu_vec4_sub(b, a), {9, 18, 27, 36}), "vec4_sub");
    check(vec4_eq(xpu_vec4_mul(a, b), {10, 40, 90, 160}), "vec4_mul");
    check(vec4_eq(xpu_vec4_scale(a, 2), {2, 4, 6, 8}), "vec4_scale");
    check(std::fabs(xpu_vec4_dot(a, b) - 300.0f) < 1e-3f, "vec4_dot");
    check(std::fabs(xpu_vec4_length({3, 4, 0, 0}) - 5.0f) < 1e-3f, "vec4_length");

    XpuVec4 n = xpu_vec4_normalize({3, 4, 0, 0});
    check(std::fabs(n.x - 0.6f) < 1e-5f && std::fabs(n.y - 0.8f) < 1e-5f, "vec4_normalize");

    XpuVec4 c = xpu_vec4_cross_xyz({1, 0, 0, 0}, {0, 1, 0, 0});
    check(vec4_eq(c, {0, 0, 1, 0}), "vec4_cross");

    XpuVec4 l = xpu_vec4_lerp({0, 0, 0, 0}, {10, 20, 30, 40}, 0.5f);
    check(vec4_eq(l, {5, 10, 15, 20}), "vec4_lerp");

    /* Matrix tests */
    XpuMat4 I = xpu_mat4_identity();
    check(I.m[0] == 1.0f && I.m[5] == 1.0f && I.m[10] == 1.0f && I.m[15] == 1.0f, "mat4_identity");

    XpuMat4 T = xpu_mat4_translate(1, 2, 3);
    XpuVec4 p = xpu_mat4_transform(T, {0, 0, 0, 1});
    check(vec4_eq(p, {1, 2, 3, 1}), "mat4_translate");

    XpuMat4 R90 = xpu_mat4_rotate_z(1.5707963f);
    XpuVec4 r = xpu_mat4_transform(R90, {1, 0, 0, 1});
    check(std::fabs(r.x) < 1e-4f && std::fabs(r.y - 1.0f) < 1e-4f, "mat4_rotate_z");

    /* Matrix multiply: M * I = M */
    XpuMat4 MI = xpu_mat4_mul(T, I);
    bool mm_ok = true;
    for (int i = 0; i < 16; ++i) if (std::fabs(MI.m[i] - T.m[i]) > 1e-6f) mm_ok = false;
    check(mm_ok, "mat4_mul_identity");

    /* Matrix inverse: M * M^-1 = I */
    XpuMat4 M = xpu_mat4_mul(xpu_mat4_translate(1, 2, 3), xpu_mat4_rotate_y(0.5f));
    XpuMat4 Minv = xpu_mat4_inverse(M);
    XpuMat4 prod = xpu_mat4_mul(M, Minv);
    bool inv_ok = true;
    for (int i = 0; i < 16; ++i) {
        float expected = (i == 0 || i == 5 || i == 10 || i == 15) ? 1.0f : 0.0f;
        if (std::fabs(prod.m[i] - expected) > 1e-3f) inv_ok = false;
    }
    check(inv_ok, "mat4_inverse");

    /* Perspective matrix sanity: point on near plane maps to NDC z = -1 */
    XpuMat4 P = xpu_mat4_perspective(1.5708f, 1.0f, 0.1f, 100.0f);
    XpuVec4 near_pt = xpu_mat4_transform(P, {0, 0, -0.1f, 1});
    /* After perspective divide, z/w should be -1.0 (GL convention) */
    float ndc_z = near_pt.z / near_pt.w;
    check(std::fabs(ndc_z + 1.0f) < 1e-3f, "perspective_near_z_ndc");

    /* Batched transform: 4 vertices through identity matrix */
    XpuVec4 src[4] = {{1, 2, 3, 1}, {4, 5, 6, 1}, {7, 8, 9, 1}, {10, 11, 12, 1}};
    XpuVec4 dst[4];
    xpu_transform_vertices(&I, src, dst, 4);
    bool bt_ok = true;
    for (int i = 0; i < 4; ++i) {
        if (!vec4_eq(src[i], dst[i], 1e-4f)) bt_ok = false;
    }
    check(bt_ok, "transform_vertices_identity");

    if (g_failures) {
        std::fprintf(stderr, "\n%d test(s) failed\n", g_failures);
        return 1;
    }
    std::printf("\nAll tests passed!\n");
    return 0;
}
