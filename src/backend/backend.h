/**
 * XPU - src/backend/backend.h
 *
 * Backend interface - the contract every backend (software, OpenGL ES,
 * Vulkan, ...) must implement. The core library routes every API call
 * through this vtable.
 */

#ifndef XPU_BACKEND_H
#define XPU_BACKEND_H

#include "xpu/xpu.h"
#include "xpu/xpu_device.h"
#include "xpu/xpu_buffer.h"
#include "xpu/xpu_shader.h"
#include "xpu/xpu_pipeline.h"
#include "xpu/xpu_command.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct xpu_backend_vtable {
    /* Instance / device */
    XpuResult (*create_instance)(void** out_state);
    void      (*destroy_instance)(void* state);
    XpuResult (*enumerate_physical_devices)(void* state, uint32_t* count, XpuPhysicalDeviceInfo* out);
    XpuResult (*create_device)(void* state, uint32_t phys_index, const XpuDeviceCreateInfo*, void** out_dev);
    void      (*destroy_device)(void* dev);
    XpuResult (*get_queue)(void* dev, uint32_t idx, void** out_queue);
    XpuResult (*queue_wait_idle)(void* queue);
    XpuResult (*device_wait_idle)(void* dev);
    XpuResult (*set_perf_hint)(void* dev, XpuPerfHint hint);
    XpuResult (*get_memory_props)(void* dev, XpuMemoryProperties* out);
    int       (*format_supported)(void* dev, XpuFormat fmt);

    /* Buffers / images */
    XpuResult (*create_buffer)(void* dev, const XpuBufferCreateInfo*, XpuBuffer* out);
    void      (*destroy_buffer)(XpuBuffer buf);
    XpuResult (*map_buffer)(XpuBuffer buf, void** out);
    XpuResult (*unmap_buffer)(XpuBuffer buf);
    XpuResult (*update_buffer)(XpuBuffer buf, xpu_size off, xpu_size sz, const void* data);

    XpuResult (*create_image)(void* dev, const XpuImageCreateInfo*, XpuImage* out);
    void      (*destroy_image)(XpuImage img);

    /* Shaders / pipelines */
    XpuResult (*create_shader)(void* dev, const XpuShaderCreateInfo*, XpuShader* out);
    void      (*destroy_shader)(XpuShader sh);
    XpuResult (*create_graphics_pipeline)(void* dev, const XpuGraphicsPipelineCreateInfo*, XpuPipeline* out);
    XpuResult (*create_compute_pipeline)(void* dev, const XpuComputePipelineCreateInfo*, XpuPipeline* out);
    void      (*destroy_pipeline)(XpuPipeline p);

    /* Command buffers */
    XpuResult (*create_command_buffer)(void* dev, XpuCommandBufferLevel lvl, XpuCommandBuffer* out);
    void      (*destroy_command_buffer)(XpuCommandBuffer cmd);
    XpuResult (*begin_command_buffer)(XpuCommandBuffer cmd);
    XpuResult (*end_command_buffer)(XpuCommandBuffer cmd);
    XpuResult (*reset_command_buffer)(XpuCommandBuffer cmd);
    XpuResult (*cmd_bind_pipeline)(XpuCommandBuffer cmd, XpuPipeline p);
    XpuResult (*cmd_bind_vertex_buffer)(XpuCommandBuffer cmd, uint32_t bind, XpuBuffer buf, xpu_size off);
    XpuResult (*cmd_bind_index_buffer)(XpuCommandBuffer cmd, XpuBuffer buf, xpu_size off, int idx32);
    XpuResult (*cmd_bind_uniform)(XpuCommandBuffer cmd, uint32_t set, uint32_t binding, XpuBuffer buf, xpu_size off, xpu_size sz);
    XpuResult (*cmd_bind_storage)(XpuCommandBuffer cmd, uint32_t set, uint32_t binding, XpuBuffer buf, xpu_size off, xpu_size sz);
    XpuResult (*cmd_set_viewport)(XpuCommandBuffer cmd, float x, float y, float w, float h, float zn, float zf);
    XpuResult (*cmd_set_scissor)(XpuCommandBuffer cmd, int32_t x, int32_t y, uint32_t w, uint32_t h);
    XpuResult (*cmd_draw)(XpuCommandBuffer cmd, uint32_t v, uint32_t i, uint32_t fv, uint32_t fi);
    XpuResult (*cmd_draw_indexed)(XpuCommandBuffer cmd, uint32_t idx, uint32_t inst, uint32_t fi, int32_t vo, uint32_t finst);
    XpuResult (*cmd_dispatch)(XpuCommandBuffer cmd, uint32_t x, uint32_t y, uint32_t z);
    XpuResult (*cmd_clear_color)(XpuCommandBuffer cmd, XpuImage img, const float rgba[4]);
    XpuResult (*cmd_pipeline_barrier)(XpuCommandBuffer cmd);
    XpuResult (*queue_submit)(void* queue, const XpuSubmitInfo* info);
} xpu_backend_vtable;

/* Probe a specific backend. Returns true and fills vtable+state if available. */
int xpu_backend_probe(XpuBackendType type, xpu_backend_vtable* out_vtable, void** out_state);

/* Human-readable backend name */
const char* xpu_backend_name(XpuBackendType type);

#ifdef __cplusplus
}
#endif

#endif /* XPU_BACKEND_H */
