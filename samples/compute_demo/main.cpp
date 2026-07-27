/**
 * XPU - samples/compute_demo/main.cpp
 *
 * Compute shader demo - uses XPU to dispatch a 1D grid of compute work.
 */

#include "xpu/xpu.h"
#include "xpu/xpu_buffer.h"
#include "xpu/xpu_shader.h"
#include "xpu/xpu_pipeline.h"
#include "xpu/xpu_command.h"
#include "xpu/xpu_math.h"

#include <cstdio>
#include <cstring>

static const char* kComputeSrc = R"GLSL(
#version 430 core
layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;
layout(std430, binding = 0) buffer InputBuffer  { float in_data[];  };
layout(std430, binding = 1) buffer OutputBuffer { float out_data[]; };
void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= in_data.length()) return;
    out_data[idx] = in_data[idx] * in_data[idx] + 0.5f;
}
)GLSL";

int main() {
    std::printf("=== XPU Compute Demo ===\n");

    XpuInstanceCreateInfo ici{};
    ici.preferred_backend = XPU_BACKEND_AUTO;
    ici.app_name = "compute_demo";

    XpuInstance inst = nullptr;
    if (xpuCreateInstance(&ici, &inst) != XPU_SUCCESS) {
        std::fprintf(stderr, "Failed to create instance\n");
        return 1;
    }

    uint32_t n = 0;
    xpuEnumeratePhysicalDevices(inst, &n, nullptr);
    XpuPhysicalDevice phys = nullptr;
    xpuEnumeratePhysicalDevices(inst, &n, &phys);

    XpuDeviceCreateInfo dci{};
    dci.physical_device = phys;
    dci.queue_count = 1;
    dci.enable_compute = XPU_TRUE;
    XpuDevice dev = nullptr;
    xpuCreateDevice(phys, &dci, &dev);

    XpuQueue q = nullptr;
    xpuGetDeviceQueue(dev, 0, &q);

    /* Create input + output storage buffers */
    const uint32_t COUNT = 1024;
    XpuBufferCreateInfo bci{};
    bci.device = dev;
    bci.size = sizeof(float) * COUNT;
    bci.usage = XPU_BUFFER_USAGE_STORAGE | XPU_BUFFER_USAGE_TRANSFER_DST;
    bci.memory = XPU_MEMORY_USAGE_CPU_TO_GPU;
    bci.persistently_mapped = XPU_TRUE;

    XpuBuffer in_buf = nullptr, out_buf = nullptr;
    xpuCreateBuffer(&bci, &in_buf);
    bci.usage = XPU_BUFFER_USAGE_STORAGE;
    xpuCreateBuffer(&bci, &out_buf);

    /* Fill input with test data */
    float* in_data = nullptr;
    xpuMapBuffer(in_buf, (void**)&in_data);
    for (uint32_t i = 0; i < COUNT; ++i) in_data[i] = (float)i;
    xpuUnmapBuffer(in_buf);

    /* Build compute pipeline */
    XpuShaderCreateInfo sci{};
    sci.device = dev;
    sci.source_type = XPU_SHADER_SOURCE_GLSL;
    sci.stage = XPU_SHADER_STAGE_COMPUTE;
    sci.source = kComputeSrc;
    sci.source_size = std::strlen(kComputeSrc);
    sci.entry_point = "main";
    XpuShader cs = nullptr;
    xpuCreateShader(&sci, &cs);

    XpuComputePipelineCreateInfo pci{};
    pci.device = dev;
    pci.compute_shader = cs;
    XpuPipeline pipeline = nullptr;
    xpuCreateComputePipeline(&pci, &pipeline);

    /* Dispatch */
    XpuCommandBufferCreateInfo cci{};
    cci.device = dev;
    cci.level = XPU_COMMAND_BUFFER_LEVEL_PRIMARY;
    XpuCommandBuffer cmd = nullptr;
    xpuCreateCommandBuffer(&cci, &cmd);

    xpuBeginCommandBuffer(cmd);
    xpuCmdBindPipeline(cmd, pipeline);
    xpuCmdBindStorageBuffer(cmd, 0, 0, in_buf, 0, sizeof(float) * COUNT);
    xpuCmdBindStorageBuffer(cmd, 0, 1, out_buf, 0, sizeof(float) * COUNT);
    xpuCmdDispatch(cmd, (COUNT + 63) / 64, 1, 1);
    xpuCmdPipelineBarrier(cmd);
    xpuEndCommandBuffer(cmd);

    XpuSubmitInfo si{};
    si.command_buffers = &cmd;
    si.command_buffer_count = 1;
    xpuQueueSubmit(q, &si);
    xpuQueueWaitIdle(q);

    /* Read back results */
    float* out_data = nullptr;
    xpuMapBuffer(out_buf, (void**)&out_data);
    std::printf("Sample outputs:\n");
    for (int i = 0; i < 4; ++i) {
        std::printf("  in[%d] = %.2f, out[%d] = %.2f (expected %.2f)\n",
                      i, (float)i, i, out_data[i], (float)i * (float)i + 0.5f);
    }
    xpuUnmapBuffer(out_buf);

    /* Cleanup */
    xpuDestroyCommandBuffer(cmd);
    xpuDestroyPipeline(pipeline);
    xpuDestroyShader(cs);
    xpuDestroyBuffer(out_buf);
    xpuDestroyBuffer(in_buf);
    xpuDestroyDevice(dev);
    xpuDestroyInstance(inst);
    std::printf("=== Done ===\n");
    return 0;
}
