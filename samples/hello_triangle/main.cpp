/**
 * XPU - samples/hello_triangle/main.cpp
 *
 * Minimal "Hello Triangle" demo using the XPU API.
 * This sample creates an instance, picks a device, uploads vertex data,
 * builds a graphics pipeline with GLSL shaders, and records a draw
 * command into a command buffer.
 *
 * Build with the top-level CMake target `xpu_hello_triangle`.
 */

#include "xpu/xpu.h"
#include "xpu/xpu_device.h"
#include "xpu/xpu_buffer.h"
#include "xpu/xpu_shader.h"
#include "xpu/xpu_pipeline.h"
#include "xpu/xpu_command.h"
#include "xpu/xpu_math.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

static const char* kVertexShaderSrc = R"GLSL(
#version 330 core
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec4 a_color;
layout(location = 0) uniform mat4 u_mvp;
out vec4 v_color;
void main() {
    gl_Position = u_mvp * vec4(a_position, 1.0);
    v_color = a_color;
}
)GLSL";

static const char* kFragmentShaderSrc = R"GLSL(
#version 330 core
in vec4 v_color;
out vec4 frag_color;
void main() {
    frag_color = v_color;
}
)GLSL";

int main() {
    std::printf("=== XPU Hello Triangle ===\n");
    std::printf("API version : %s\n", xpuGetVersionString());
    std::printf("Build info  : %s\n", xpuGetBuildInfo());

    XpuCpuArch arch = xpu_math_detect_cpu_arch();
    std::printf("Detected CPU: %s\n", xpu_math_arch_name(arch));

    /* Create instance */
    XpuInstanceCreateInfo ici{};
    ici.preferred_backend   = XPU_BACKEND_AUTO;
    ici.enable_validation   = XPU_TRUE;
    ici.enable_debug        = XPU_TRUE;
    ici.app_name            = "hello_triangle";
    ici.app_version         = 1;
    ici.engine_name         = "xpu_sample";
    ici.engine_version      = 1;

    XpuInstance instance = nullptr;
    XpuResult r = xpuCreateInstance(&ici, &instance);
    if (r != XPU_SUCCESS) {
        std::fprintf(stderr, "xpuCreateInstance failed: %s\n", xpuResultString(r));
        return 1;
    }
    std::printf("Active backend: %s\n", xpuBackendName(xpuGetActiveBackend(instance)));

    /* Enumerate physical devices */
    uint32_t gpu_count = 0;
    xpuEnumeratePhysicalDevices(instance, &gpu_count, nullptr);
    std::printf("Physical devices found: %u\n", gpu_count);
    if (gpu_count == 0) {
        std::fprintf(stderr, "No physical devices!\n");
        xpuDestroyInstance(instance);
        return 1;
    }

    XpuPhysicalDevice phys = nullptr;
    xpuEnumeratePhysicalDevices(instance, &gpu_count, &phys);

    XpuPhysicalDeviceInfo pinfo{};
    xpuGetPhysicalDeviceInfo(phys, &pinfo);
    std::printf("Using device: %s (driver %s)\n", pinfo.device_name, pinfo.driver_name);

    /* Create logical device + queue */
    XpuDeviceCreateInfo dci{};
    dci.physical_device = phys;
    dci.queue_count     = 1;
    dci.queue_priorities[0] = 1.0f;
    dci.enable_compute  = XPU_TRUE;
    XpuDevice device = nullptr;
    r = xpuCreateDevice(phys, &dci, &device);
    if (r != XPU_SUCCESS) {
        std::fprintf(stderr, "xpuCreateDevice failed: %s\n", xpuResultString(r));
        xpuDestroyInstance(instance);
        return 1;
    }

    XpuQueue queue = nullptr;
    xpuGetDeviceQueue(device, 0, &queue);

    XpuMemoryProperties mem{};
    xpuGetDeviceMemoryProperties(device, &mem);
    std::printf("Memory: device_local=%llu MB, host_visible=%llu MB\n",
                  (unsigned long long)mem.device_local_bytes / (1024 * 1024),
                  (unsigned long long)mem.host_visible_bytes / (1024 * 1024));

    /* Build MVP matrix using XPU math (SIMD-accelerated) */
    XpuMat4 model      = xpu_mat4_rotate_y(0.5f);
    XpuMat4 view       = xpu_mat4_lookat({0, 0, 3}, {0, 0, 0}, {0, 1, 0});
    XpuMat4 projection = xpu_mat4_perspective(1.0472f, 16.0f / 9.0f, 0.1f, 100.0f);
    XpuMat4 mvp = xpu_mat4_mul(projection, xpu_mat4_mul(view, model));

    /* Upload vertex data */
    struct Vertex { float x, y, z; float r, g, b, a; };
    static const Vertex kVertices[3] = {
        { 0.0f,  0.6f, 0.0f,  1.0f, 0.0f, 0.0f, 1.0f},
        {-0.6f, -0.4f, 0.0f,  0.0f, 1.0f, 0.0f, 1.0f},
        { 0.6f, -0.4f, 0.0f,  0.0f, 0.0f, 1.0f, 1.0f},
    };

    XpuBufferCreateInfo bci{};
    bci.device              = device;
    bci.size                = sizeof(kVertices);
    bci.usage               = XPU_BUFFER_USAGE_VERTEX | XPU_BUFFER_USAGE_TRANSFER_DST;
    bci.memory              = XPU_MEMORY_USAGE_CPU_TO_GPU;
    bci.persistently_mapped = XPU_TRUE;

    XpuBuffer vbo = nullptr;
    r = xpuCreateBuffer(&bci, &vbo);
    if (r != XPU_SUCCESS) {
        std::fprintf(stderr, "xpuCreateBuffer failed: %s\n", xpuResultString(r));
        return 1;
    }
    xpuUpdateBuffer(vbo, 0, sizeof(kVertices), kVertices);

    /* Uniform buffer for the MVP */
    XpuBufferCreateInfo ubi{};
    ubi.device              = device;
    ubi.size                = sizeof(XpuMat4);
    ubi.usage               = XPU_BUFFER_USAGE_UNIFORM;
    ubi.memory              = XPU_MEMORY_USAGE_CPU_TO_GPU;
    ubi.persistently_mapped = XPU_TRUE;
    XpuBuffer ubo = nullptr;
    xpuCreateBuffer(&ubi, &ubo);
    xpuUpdateBuffer(ubo, 0, sizeof(XpuMat4), &mvp);

    /* Create shaders */
    XpuShaderCreateInfo vsci{};
    vsci.device       = device;
    vsci.source_type  = XPU_SHADER_SOURCE_GLSL;
    vsci.stage        = XPU_SHADER_STAGE_VERTEX;
    vsci.source       = kVertexShaderSrc;
    vsci.source_size  = std::strlen(kVertexShaderSrc);
    vsci.entry_point  = "main";
    XpuShader vs = nullptr;
    xpuCreateShader(&vsci, &vs);

    XpuShaderCreateInfo fsci{};
    fsci.device       = device;
    fsci.source_type  = XPU_SHADER_SOURCE_GLSL;
    fsci.stage        = XPU_SHADER_STAGE_FRAGMENT;
    fsci.source       = kFragmentShaderSrc;
    fsci.source_size  = std::strlen(kFragmentShaderSrc);
    fsci.entry_point  = "main";
    XpuShader fs = nullptr;
    xpuCreateShader(&fsci, &fs);

    /* Pipeline */
    XpuVertexInputBinding binding{};
    binding.binding     = 0;
    binding.stride      = sizeof(Vertex);
    binding.per_instance = XPU_FALSE;

    XpuVertexInputAttribute attrs[2] = {};
    attrs[0].location = 0;
    attrs[0].binding  = 0;
    attrs[0].offset   = 0;
    attrs[0].format   = XPU_FORMAT_RG32_FLOAT;
    attrs[1].location = 1;
    attrs[1].binding  = 0;
    attrs[1].offset   = sizeof(float) * 3;
    attrs[1].format   = XPU_FORMAT_RGBA32_FLOAT;

    XpuGraphicsPipelineCreateInfo pci{};
    pci.device                = device;
    pci.vertex_shader         = vs;
    pci.fragment_shader       = fs;
    pci.topology              = XPU_TOPOLOGY_TRIANGLE_LIST;
    pci.vertex_input.bindings = &binding;
    pci.vertex_input.binding_count = 1;
    pci.vertex_input.attributes = attrs;
    pci.vertex_input.attribute_count = 2;
    pci.rasterizer.polygon_mode = XPU_POLYGON_MODE_FILL;
    pci.rasterizer.cull_mode    = XPU_CULL_MODE_BACK;
    pci.rasterizer.line_width   = 1.0f;
    pci.depth_stencil.depth_test  = XPU_FALSE;
    pci.depth_stencil.depth_write = XPU_FALSE;
    pci.color_format         = XPU_FORMAT_BGRA8_UNORM;
    pci.depth_format         = XPU_FORMAT_D32_FLOAT;
    pci.dynamic_viewport     = XPU_TRUE;
    pci.dynamic_scissor      = XPU_TRUE;

    XpuPipeline pipeline = nullptr;
    r = xpuCreateGraphicsPipeline(&pci, &pipeline);
    if (r != XPU_SUCCESS) {
        std::fprintf(stderr, "xpuCreateGraphicsPipeline failed: %s\n", xpuResultString(r));
        return 1;
    }

    /* Record command buffer */
    XpuCommandBufferCreateInfo cci{};
    cci.device = device;
    cci.level  = XPU_COMMAND_BUFFER_LEVEL_PRIMARY;
    XpuCommandBuffer cmd = nullptr;
    xpuCreateCommandBuffer(&cci, &cmd);

    xpuBeginCommandBuffer(cmd);
    xpuCmdBindPipeline(cmd, pipeline);
    xpuCmdBindVertexBuffer(cmd, 0, vbo, 0);
    xpuCmdBindUniformBuffer(cmd, 0, 0, ubo, 0, sizeof(XpuMat4));
    xpuCmdSetViewport(cmd, 0, 0, 1280, 720, 0.0f, 1.0f);
    xpuCmdSetScissor(cmd, 0, 0, 1280, 720);
    xpuCmdDraw(cmd, 3, 1, 0, 0);
    xpuEndCommandBuffer(cmd);

    /* Submit */
    XpuSubmitInfo si{};
    si.command_buffers = &cmd;
    si.command_buffer_count = 1;
    xpuQueueSubmit(queue, &si);
    xpuQueueWaitIdle(queue);

    std::printf("Hello triangle submitted successfully!\n");

    /* Test the SIMD math path - transforms 1M vertices to demonstrate throughput */
    const xpu_size N = 1024 * 1024;
    XpuVec4* in  = (XpuVec4*)std::malloc(sizeof(XpuVec4) * N);
    XpuVec4* out = (XpuVec4*)std::malloc(sizeof(XpuVec4) * N);
    for (xpu_size i = 0; i < N; ++i) {
        in[i] = XpuVec4{(float)i, (float)(i * 2), (float)(i * 3), 1.0f};
    }
    xpu_transform_vertices(&mvp, in, out, N);
    std::printf("Transformed %zu vertices with %s path\n", (size_t)N, xpu_math_arch_name(arch));
    std::printf("Sample output: out[0] = (%.3f, %.3f, %.3f, %.3f)\n",
                  out[0].x, out[0].y, out[0].z, out[0].w);
    std::free(in);
    std::free(out);

    /* Cleanup */
    xpuDestroyCommandBuffer(cmd);
    xpuDestroyPipeline(pipeline);
    xpuDestroyShader(fs);
    xpuDestroyShader(vs);
    xpuDestroyBuffer(ubo);
    xpuDestroyBuffer(vbo);
    xpuDestroyDevice(device);
    xpuDestroyInstance(instance);

    std::printf("=== Done ===\n");
    return 0;
}
