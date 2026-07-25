/**
 * XPU - samples/render_daemon/main.cpp
 *
 * A background render daemon that runs continuously - similar to a
 * screen recorder. It spins up the XPU software backend, sets up a
 * render target, and keeps rendering frames forever.
 *
 * Each frame:
 *   1. Clears the framebuffer
 *   2. Updates a rotating-cube model matrix
 *   3. Rasterizes 12 triangles (cube faces) using the real software rasterizer
 *   4. Saves the result to render_output/frame_NNNN.bmp (one per second)
 *
 * Usage:
 *   xpu_render_daemon                    # render forever, save a frame/sec
 *   xpu_render_daemon --max-frames 60    # stop after 60 frames
 *   xpu_render_daemon --width 1280 --height 720
 *   xpu_render_daemon --no-save          # don't save BMPs (pure benchmark)
 *
 * On Android/Termux, you can launch this in the background with:
 *   nohup ./xpu_render_daemon --width 640 --height 480 &
 *
 * Build:
 *   make samples
 *   # or
 *   make build/xpu_render_daemon
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
#include <sys/stat.h>
#include <sys/time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* A simple cube: 8 vertices, 12 triangles (36 indices) */
static const XpuVertex kCubeVertices[8] = {
    /* position (x, y, z, w)        color (r, g, b, a) */
    {{-1, -1, -1, 1}, {1, 0, 0, 1}},  /* 0: front-bottom-left  */
    {{ 1, -1, -1, 1}, {0, 1, 0, 1}},  /* 1: front-bottom-right */
    {{ 1,  1, -1, 1}, {0, 0, 1, 1}},  /* 2: front-top-right    */
    {{-1,  1, -1, 1}, {1, 1, 0, 1}},  /* 3: front-top-left     */
    {{-1, -1,  1, 1}, {1, 0, 1, 1}},  /* 4: back-bottom-left   */
    {{ 1, -1,  1, 1}, {0, 1, 1, 1}},  /* 5: back-bottom-right  */
    {{ 1,  1,  1, 1}, {1, 1, 1, 1}},  /* 6: back-top-right     */
    {{-1,  1,  1, 1}, {0.5f, 0.5f, 0.5f, 1}}, /* 7: back-top-left */
};

static const uint32_t kCubeIndices[36] = {
    /* front face */
    0, 1, 2,   0, 2, 3,
    /* back face */
    4, 6, 5,   4, 7, 6,
    /* left face */
    0, 3, 7,   0, 7, 4,
    /* right face */
    1, 5, 6,   1, 6, 2,
    /* top face */
    3, 2, 6,   3, 6, 7,
    /* bottom face */
    0, 4, 5,   0, 5, 1,
};

static double now_seconds() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

static void make_dir(const char* path) {
#ifdef _WIN32
    _mkdir(path);
#else
    mkdir(path, 0755);
#endif
}

int main(int argc, char** argv) {
    uint32_t width       = 800;
    uint32_t height      = 600;
    uint32_t max_frames  = 0;        /* 0 = infinite */
    uint32_t save_every  = 30;       /* save every N frames (30 frames @ 30fps = 1 sec) */
    int      save_bmps   = 1;
    const char* out_dir  = "render_output";

    /* Parse args */
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--width" && i + 1 < argc) {
            width = (uint32_t)std::atoi(argv[++i]);
        } else if (arg == "--height" && i + 1 < argc) {
            height = (uint32_t)std::atoi(argv[++i]);
        } else if (arg == "--max-frames" && i + 1 < argc) {
            max_frames = (uint32_t)std::atoi(argv[++i]);
        } else if (arg == "--save-every" && i + 1 < argc) {
            save_every = (uint32_t)std::atoi(argv[++i]);
        } else if (arg == "--no-save") {
            save_bmps = 0;
        } else if (arg == "--out-dir" && i + 1 < argc) {
            out_dir = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            std::printf("XPU Render Daemon - continuous background renderer\n");
            std::printf("Usage: %s [options]\n", argv[0]);
            std::printf("  --width N        framebuffer width  (default 800)\n");
            std::printf("  --height N       framebuffer height (default 600)\n");
            std::printf("  --max-frames N   stop after N frames (default: infinite)\n");
            std::printf("  --save-every N   save a BMP every N frames (default 30)\n");
            std::printf("  --no-save        don't save BMPs (benchmark mode)\n");
            std::printf("  --out-dir DIR    output directory (default: render_output)\n");
            return 0;
        }
    }

    std::printf("=== XPU Render Daemon ===\n");
    std::printf("XPU version   : %s\n", xpuGetVersionString());
    std::printf("CPU detected  : %s\n", xpu_math_arch_name(xpu_math_detect_cpu_arch()));
    std::printf("Framebuffer   : %u x %u\n", width, height);
    std::printf("Max frames    : %s\n", max_frames ? std::to_string(max_frames).c_str() : "infinite");
    std::printf("Save BMPs     : %s%s\n", save_bmps ? "yes" : "no",
                  save_bmps ? (", every " + std::to_string(save_every) + " frames").c_str() : "");
    std::printf("Output dir    : %s\n", out_dir);
    std::printf("Press Ctrl+C to stop.\n\n");

    /* Initialize XPU instance (picks best backend, falls back to software) */
    XpuInstanceCreateInfo ici{};
    ici.preferred_backend = XPU_BACKEND_AUTO;
    ici.enable_validation = XPU_FALSE;
    ici.enable_debug      = XPU_FALSE;
    ici.app_name          = "render_daemon";
    ici.engine_name       = "xpu_daemon";
    XpuInstance inst = nullptr;
    if (xpuCreateInstance(&ici, &inst) != XPU_SUCCESS) {
        std::fprintf(stderr, "Failed to create XPU instance\n");
        return 1;
    }
    std::printf("Active backend: %s\n", xpuBackendName(xpuGetActiveBackend(inst)));

    /* Create render target */
    XpuRenderTarget* rt = xpu_sw_rt_create(width, height);
    if (!rt) {
        std::fprintf(stderr, "Failed to create render target\n");
        xpuDestroyInstance(inst);
        return 1;
    }

    if (save_bmps) make_dir(out_dir);

    /* Pre-allocate transformed vertex buffer (8 verts, transformed per frame) */
    XpuVertex transformed[8];

    /* Render loop */
    uint32_t frame = 0;
    double   last_report = now_seconds();
    uint32_t frames_since_report = 0;
    double   total_start = now_seconds();

    while (max_frames == 0 || frame < max_frames) {
        double t = now_seconds() - total_start;
        float  angle = (float)(t * 0.7);

        /* Clear */
        xpu_sw_rt_clear_color(rt, 0.05f, 0.05f, 0.1f, 1.0f);
        xpu_sw_rt_clear_depth(rt, 1.0f);

        /* Build MVP using XPU's SIMD-accelerated math */
        XpuMat4 model = xpu_mat4_mul(
            xpu_mat4_rotate_y(angle),
            xpu_mat4_rotate_x(angle * 0.5f)
        );
        XpuMat4 view = xpu_mat4_lookat(
            {0, 0, 6},   /* eye */
            {0, 0, 0},   /* center */
            {0, 1, 0}    /* up */
        );
        XpuMat4 proj = xpu_mat4_perspective(
            (float)(60.0 * M_PI / 180.0),
            (float)width / (float)height,
            0.1f, 100.0f
        );
        XpuMat4 mvp = xpu_mat4_mul(proj, xpu_mat4_mul(view, model));

        /* Transform cube vertices to clip space using SIMD */
        for (int i = 0; i < 8; ++i) {
            XpuVec4 pos = xpu_mat4_transform(mvp, kCubeVertices[i].position);
            transformed[i].position = pos;
            transformed[i].color    = kCubeVertices[i].color;
        }

        /* Rasterize all 12 triangles */
        xpu_sw_rasterize_triangle_list(rt, transformed, 8, kCubeIndices, 36);

        /* Save a frame every `save_every` frames when in save mode */
        if (save_bmps && (frame % save_every == 0)) {
            char path[512];
            std::snprintf(path, sizeof(path), "%s/frame_%05u.bmp", out_dir, frame);
            if (xpu_sw_rt_save_bmp(rt, path) == 0) {
                std::printf("[frame %6u] saved %s\n", frame, path);
            } else {
                std::fprintf(stderr, "[frame %6u] failed to save %s\n", frame, path);
            }
        }

        /* FPS report every 2 seconds */
        ++frame;
        ++frames_since_report;
        double now = now_seconds();
        if (now - last_report >= 2.0) {
            double fps = frames_since_report / (now - last_report);
            std::printf("[frame %6u] FPS: %.1f  total: %.1fs  backend: %s\n",
                          frame, fps, now - total_start,
                          xpuBackendName(xpuGetActiveBackend(inst)));
            last_report = now;
            frames_since_report = 0;
        }
    }

    double elapsed = now_seconds() - total_start;
    std::printf("\n=== Render daemon complete ===\n");
    std::printf("Total frames : %u\n", frame);
    std::printf("Total time   : %.2f s\n", elapsed);
    std::printf("Average FPS  : %.2f\n", frame / elapsed);

    xpu_sw_rt_destroy(rt);
    xpuDestroyInstance(inst);
    return 0;
}
