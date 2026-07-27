/**
 * XPU - src/core/xpu_internal_structs.h
 *
 * INTERNAL - NOT part of the public API.
 *
 * Complete definitions of all opaque handle types. These are defined
 * here so that the various implementation files (xpu_core.cpp,
 * xpu_buffer.cpp, xpu_shader.cpp, xpu_pipeline.cpp, xpu_command.cpp,
 * backend_software.cpp) can share the same memory layout.
 *
 * DO NOT include this header from public API headers.
 */

#ifndef XPU_INTERNAL_STRUCTS_H
#define XPU_INTERNAL_STRUCTS_H

#include "xpu/xpu.h"
#include "xpu/xpu_device.h"
#include "xpu/xpu_buffer.h"
#include "xpu/xpu_shader.h"
#include "xpu/xpu_pipeline.h"
#include "xpu/xpu_command.h"
#include "../backend/backend.h"

#include <atomic>
#include <string>
#include <vector>

/* ------------------------------------------------------------------ */
/* Instance + physical device                                          */
/* ------------------------------------------------------------------ */
struct XpuInstance_T {
    XpuBackendType          active_backend;
    xpu_backend_vtable      vtable;
    void*                   backend_state;
    XpuPhysicalDeviceInfo*  physical_devices;
    uint32_t                physical_device_count;
};

/* ------------------------------------------------------------------ */
/* Device + queue                                                      */
/* ------------------------------------------------------------------ */
struct XpuDevice_T {
    XpuInstance          instance;
    XpuPhysicalDevice    physical;
    XpuPhysicalDeviceInfo info;
    void*                backend_device;
    XpuPerfHint          perf_hint;
};

struct XpuQueue_T {
    XpuDevice device;
    uint32_t  index;
    void*     backend_queue;
};

/* ------------------------------------------------------------------ */
/* Context                                                             */
/* ------------------------------------------------------------------ */
struct XpuContext_T {
    XpuDevice device;
};

/* ------------------------------------------------------------------ */
/* Buffer + Image (software-backend layout; backends may reinterpret) */
/* ------------------------------------------------------------------ */
struct XpuBuffer_T {
    XpuDevice       device;
    xpu_size        size;
    uint32_t        usage;
    XpuMemoryUsage  memory;
    void*           data;
    int             persistently_mapped;
};

struct XpuImage_T {
    XpuFormat  format;
    uint32_t   width;
    uint32_t   height;
    uint32_t   depth;
    uint32_t   mip_levels;
    uint32_t   array_layers;
    void*      data;
    xpu_size   size_bytes;
};

/* ------------------------------------------------------------------ */
/* Shader + Pipeline                                                   */
/* ------------------------------------------------------------------ */
struct XpuShader_T {
    XpuShaderStage stage;
    std::string    source;
    std::string    entry;
    std::string    log;
};

struct XpuPipeline_T {
    XpuShader vertex;
    XpuShader fragment;
    XpuShader compute;
    XpuPrimitiveTopology topology;
    XpuGraphicsPipelineCreateInfo create_info;
};

/* ------------------------------------------------------------------ */
/* Command buffer                                                      */
/* ------------------------------------------------------------------ */
struct XpuCommandBuffer_T {
    XpuDevice device;
    XpuCommandBufferLevel level;
    bool recording;
    bool has_pipeline;
    XpuPipeline current_pipeline;
    XpuBuffer   bound_vertex_buffers[8];
    XpuBuffer   bound_index_buffer;
    xpu_bool    index_is_u32;
    struct Viewport { float x, y, w, h, zn, zf; } vp;
    struct Scissor  { int32_t x, y; uint32_t w, h; } sc;
    std::vector<std::pair<uint32_t, XpuBuffer>> recorded;
};

#endif /* XPU_INTERNAL_STRUCTS_H */
