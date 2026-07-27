/**
 * XPU - samples/benchmark/main.cpp
 *
 * Performance benchmark for XPU. Useful to measure how fast XPU's
 * software renderer runs on your specific device.
 *
 * Tests:
 *   1. SIMD math throughput - 1M vertex transforms
 *   2. Triangle rasterization throughput - 10K triangles per frame
 *   3. Full frame rate (clear + transform + rasterize)
 *
 * Usage:
 *   xpu_benchmark                  # run all benchmarks
 *   xpu_benchmark --frames 1000    # specify frame count
 *   xpu_benchmark --quick          # quick 30-frame test
 */

#include "xpu/xpu.h"
#include "xpu/xpu_math.h"
#include "../../src/backend/sw_rasterizer.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <ctime>
#include <string>
#include <sys/time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static double now_seconds() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

/* Generate random colorful vertices */
static void gen_vertices(XpuVertex* v, uint32_t count, uint32_t seed) {
    /* Simple LCG for reproducibility */
    uint32_t state = seed;
    auto next = [&]() {
        state = state * 1103515245u + 12345u;
        return (state >> 8) / 16777216.0f;  /* 0..1 */
    };
    for (uint32_t i = 0; i < count; ++i) {
        v[i].position = XpuVec4{
            (next() - 0.5f) * 2.0f,
            (next() - 0.5f) * 2.0f,
            (next() - 0.5f) * 2.0f,
            1.0f
        };
        v[i].color = XpuVec4{next(), next(), next(), 1.0f};
    }
}

int main(int argc, char** argv) {
    uint32_t frame_count = 100;
    int      quick_mode  = 0;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--frames" && i + 1 < argc) {
            frame_count = (uint32_t)std::atoi(argv[++i]);
        } else if (arg == "--quick") {
            quick_mode = 1;
            frame_count = 30;
        } else if (arg == "--help" || arg == "-h") {
            std::printf("XPU Benchmark - measure rendering throughput\n");
            std::printf("Usage: %s [options]\n", argv[0]);
            std::printf("  --frames N   run N frames (default 100)\n");
            std::printf("  --quick      quick 30-frame test\n");
            return 0;
        }
    }

    std::printf("=== XPU Benchmark ===\n");
    std::printf("XPU version   : %s\n", xpuGetVersionString());
    std::printf("CPU detected  : %s\n", xpu_math_arch_name(xpu_math_detect_cpu_arch()));

    /* Print CPU info from /proc/cpuinfo if available */
    std::FILE* cpuinfo = std::fopen("/proc/cpuinfo", "r");
    if (cpuinfo) {
        char line[256];
        while (std::fgets(line, sizeof(line), cpuinfo)) {
            if (std::strncmp(line, "Hardware", 8) == 0 ||
                std::strncmp(line, "model name", 10) == 0) {
                std::printf("CPU info      : %s", line + std::strcspn(line, ":") + 2);
                break;
            }
        }
        std::fclose(cpuinfo);
    }
    int cores = 0;
    std::FILE* f = std::fopen("/proc/cpuinfo", "r");
    if (f) {
        char line[256];
        while (std::fgets(line, sizeof(line), f)) {
            if (std::strncmp(line, "processor", 9) == 0) ++cores;
        }
        std::fclose(f);
    }
    std::printf("CPU cores     : %d\n", cores > 0 ? cores : 1);

    /* --- TEST 1: SIMD math throughput --- */
    std::printf("\n--- Test 1: SIMD Math Throughput ---\n");
    const uint32_t VERT_COUNT = 1u * 1024 * 1024;  /* 1M vertices */
    XpuVec4* in_verts  = (XpuVec4*)std::malloc(sizeof(XpuVec4) * VERT_COUNT);
    XpuVec4* out_verts = (XpuVec4*)std::malloc(sizeof(XpuVec4) * VERT_COUNT);
    if (!in_verts || !out_verts) {
        std::fprintf(stderr, "Out of memory\n");
        return 1;
    }
    for (uint32_t i = 0; i < VERT_COUNT; ++i) {
        in_verts[i] = XpuVec4{(float)i, (float)(i * 2), (float)(i * 3), 1.0f};
    }
    XpuMat4 mvp = xpu_mat4_mul(
        xpu_mat4_perspective(1.0f, 1.0f, 0.1f, 100.0f),
        xpu_mat4_lookat({0, 0, 5}, {0, 0, 0}, {0, 1, 0})
    );

    double t0 = now_seconds();
    xpu_transform_vertices(&mvp, in_verts, out_verts, VERT_COUNT);
    double t1 = now_seconds();
    double math_time = t1 - t0;
    std::printf("  Transformed %u vertices in %.3f ms\n", VERT_COUNT, math_time * 1000.0);
    std::printf("  Throughput  : %.1f M vertices/sec\n", VERT_COUNT / math_time / 1e6);
    std::printf("  Per-vertex  : %.1f ns\n", math_time * 1e9 / VERT_COUNT);

    std::free(in_verts);
    std::free(out_verts);

    /* --- TEST 2: Triangle rasterization --- */
    std::printf("\n--- Test 2: Triangle Rasterization ---\n");
    const uint32_t RT_W = 640;
    const uint32_t RT_H = 480;
    XpuRenderTarget* rt = xpu_sw_rt_create(RT_W, RT_H);
    if (!rt) {
        std::fprintf(stderr, "Failed to create render target\n");
        return 1;
    }

    /* Generate a pool of random triangles */
    const uint32_t TRI_COUNT = 10000;
    XpuVertex* verts = (XpuVertex*)std::malloc(sizeof(XpuVertex) * TRI_COUNT * 3);
    gen_vertices(verts, TRI_COUNT * 3, 12345);

    /* Render frame_count frames */
    std::printf("  Rendering %u frames at %ux%u (%u triangles per frame)\n",
                  frame_count, RT_W, RT_H, TRI_COUNT);
    double frame_start = now_seconds();
    double last_report = frame_start;
    uint32_t frames_done = 0;

    for (uint32_t f = 0; f < frame_count; ++f) {
        xpu_sw_rt_clear_color(rt, 0.02f, 0.02f, 0.05f, 1.0f);
        xpu_sw_rt_clear_depth(rt, 1.0f);

        /* Rotate the triangle pool slightly each frame */
        float angle = f * 0.01f;
        XpuMat4 rot = xpu_mat4_rotate_y(angle);

        /* Transform + rasterize all triangles */
        XpuVertex transformed[3];
        for (uint32_t t = 0; t < TRI_COUNT; ++t) {
            for (int k = 0; k < 3; ++k) {
                const XpuVertex& src = verts[t * 3 + k];
                transformed[k].position = xpu_mat4_transform(rot, src.position);
                transformed[k].color    = src.color;
            }
            xpu_sw_rasterize_triangle(rt, &transformed[0], &transformed[1], &transformed[2]);
        }

        ++frames_done;
        double now = now_seconds();
        if (now - last_report >= 1.0) {
            double fps = frames_done / (now - frame_start);
            std::printf("  [frame %5u] FPS: %.1f, triangles/sec: %.1fK\n",
                          f, fps, fps * TRI_COUNT / 1000.0);
            last_report = now;
        }
    }

    double total_time = now_seconds() - frame_start;
    double avg_fps = frame_count / total_time;

    std::printf("\n--- Results ---\n");
    std::printf("  Total frames      : %u\n", frame_count);
    std::printf("  Total time        : %.2f s\n", total_time);
    std::printf("  Average FPS       : %.2f\n", avg_fps);
    std::printf("  Triangles/sec     : %.1f K (%.1f M)\n",
                  avg_fps * TRI_COUNT / 1000.0, avg_fps * TRI_COUNT / 1e6);
    std::printf("  Pixels rendered   : %u M total (%.1f M/sec)\n",
                  (uint32_t)(frame_count * TRI_COUNT * 100 / 1000000),
                  avg_fps * TRI_COUNT * 100 / 1e6);

    /* Save final frame */
    xpu_sw_rt_save_bmp(rt, "benchmark_output.bmp");
    std::printf("  Final frame saved : benchmark_output.bmp\n");

    /* --- Summary --- */
    std::printf("\n=== Summary ===\n");
    std::printf("  Math throughput    : %.1f M vert/sec (%.1f ns/vert)\n",
                  VERT_COUNT / math_time / 1e6, math_time * 1e9 / VERT_COUNT);
    std::printf("  Render throughput  : %.1f FPS @ %ux%u, %.1f K tri/sec\n",
                  avg_fps, RT_W, RT_H, avg_fps * TRI_COUNT / 1000.0);

    xpu_sw_rt_destroy(rt);
    std::free(verts);

    if (quick_mode) {
        std::printf("\nQuick benchmark complete.\n");
    } else {
        std::printf("\nFull benchmark complete.\n");
    }
    return 0;
}
