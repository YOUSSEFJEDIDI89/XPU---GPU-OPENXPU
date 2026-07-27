# XPU API Reference

This is a concise reference. For tutorial-style guidance, see the [Quick Start](../README.md#quick-start) section of the README.

## Versioning

```c
uint32_t    xpuGetVersion();          // XPU_API_VERSION
const char* xpuGetVersionString();    // "XPU 1.0.0 (OpenXPU)"
const char* xpuGetBuildInfo();        // build date + compiler
const char* xpuResultString(XpuResult);
const char* xpuBackendName(XpuBackendType);
```

## Instance

```c
typedef struct XpuInstanceCreateInfo {
    XpuBackendType preferred_backend;  // XPU_BACKEND_AUTO for auto-select
    xpu_bool       enable_validation;
    xpu_bool       enable_debug;
    const char*    app_name;
    uint32_t       app_version;
    const char*    engine_name;
    uint32_t       engine_version;
} XpuInstanceCreateInfo;

XpuResult xpuCreateInstance(const XpuInstanceCreateInfo*, XpuInstance* out);
void      xpuDestroyInstance(XpuInstance);
XpuBackendType xpuGetActiveBackend(XpuInstance);
```

## Physical devices

```c
typedef struct XpuPhysicalDeviceInfo {
    XpuVendor     vendor;
    XpuDeviceType device_type;
    XpuCpuArch    cpu_arch;
    char          device_name[256];
    char          driver_name[64];
    uint32_t      driver_version;
    uint64_t      total_memory_bytes;
    uint32_t      max_compute_units;
    uint32_t      max_workgroup_size;
    xpu_bool      supports_compute;
    xpu_bool      supports_geometry;
    xpu_bool      supports_tessellation;
    xpu_bool      supports_mesh_shaders;
} XpuPhysicalDeviceInfo;

XpuResult xpuEnumeratePhysicalDevices(XpuInstance, uint32_t* count, XpuPhysicalDevice* out);
XpuResult xpuGetPhysicalDeviceInfo(XpuPhysicalDevice, XpuPhysicalDeviceInfo* out);
```

## Device + Queue

```c
typedef struct XpuDeviceCreateInfo {
    XpuPhysicalDevice physical_device;
    uint32_t          queue_count;
    float             queue_priorities[4];
    xpu_bool          enable_compute;
} XpuDeviceCreateInfo;

XpuResult xpuCreateDevice(XpuPhysicalDevice, const XpuDeviceCreateInfo*, XpuDevice* out);
void      xpuDestroyDevice(XpuDevice);
XpuResult xpuGetDeviceQueue(XpuDevice, uint32_t index, XpuQueue* out);
XpuResult xpuQueueWaitIdle(XpuQueue);
XpuResult xpuDeviceWaitIdle(XpuDevice);

// Memory + format queries
XpuResult xpuGetDeviceMemoryProperties(XpuDevice, XpuMemoryProperties* out);
xpu_bool  xpuGetFormatSupport(XpuDevice, XpuFormat);
XpuResult xpuSetDevicePerfHint(XpuDevice, XpuPerfHint);
```

## Buffer

```c
typedef struct XpuBufferCreateInfo {
    XpuDevice       device;
    xpu_size        size;
    uint32_t        usage;     // bitmask of XpuBufferUsage
    XpuMemoryUsage  memory;
    xpu_bool        persistently_mapped;
} XpuBufferCreateInfo;

XpuResult xpuCreateBuffer(const XpuBufferCreateInfo*, XpuBuffer* out);
void      xpuDestroyBuffer(XpuBuffer);
xpu_size  xpuGetBufferSize(XpuBuffer);
XpuResult xpuMapBuffer(XpuBuffer, void** out);
XpuResult xpuUnmapBuffer(XpuBuffer);
XpuResult xpuUpdateBuffer(XpuBuffer, xpu_size offset, xpu_size size, const void* data);
XpuResult xpuFlushBufferRange(XpuBuffer, xpu_size offset, xpu_size size);
XpuResult xpuInvalidateBufferRange(XpuBuffer, xpu_size offset, xpu_size size);
```

## Image (texture)

```c
typedef struct XpuImageCreateInfo {
    XpuDevice  device;
    XpuFormat  format;
    uint32_t   width, height, depth;
    uint32_t   mip_levels;
    uint32_t   array_layers;
    xpu_bool   storage;
    xpu_bool   sampled;
    xpu_bool   render_target;
    xpu_bool   depth_stencil;
} XpuImageCreateInfo;

XpuResult xpuCreateImage(const XpuImageCreateInfo*, XpuImage* out);
void      xpuDestroyImage(XpuImage);
XpuResult xpuUpdateImage(XpuImage, uint32_t mip, uint32_t layer, const void* data, xpu_size size);
```

## Shader

```c
typedef struct XpuShaderCreateInfo {
    XpuDevice             device;
    XpuShaderSourceType   source_type;  // GLSL / SPIRV / XPU_IL / HLSL
    XpuShaderStage        stage;
    const char*           source;
    xpu_size              source_size;
    const char*           entry_point;
    const char* const*    defines;
    uint32_t              define_count;
} XpuShaderCreateInfo;

XpuResult  xpuCreateShader(const XpuShaderCreateInfo*, XpuShader* out);
void       xpuDestroyShader(XpuShader);
const char* xpuGetShaderLog(XpuShader);
```

## Pipeline

```c
// Graphics
typedef struct XpuGraphicsPipelineCreateInfo {
    XpuDevice                device;
    XpuShader                vertex_shader;
    XpuShader                fragment_shader;
    XpuShader                geometry_shader;
    XpuShader                compute_shader;
    XpuPrimitiveTopology     topology;
    XpuVertexInputState      vertex_input;
    XpuRasterizerState       rasterizer;
    XpuDepthStencilState     depth_stencil;
    XpuColorBlendState       color_blend;
    XpuFormat                color_format;
    XpuFormat                depth_format;
    xpu_bool                 dynamic_viewport;
    xpu_bool                 dynamic_scissor;
} XpuGraphicsPipelineCreateInfo;

XpuResult xpuCreateGraphicsPipeline(const XpuGraphicsPipelineCreateInfo*, XpuPipeline* out);

// Compute
typedef struct XpuComputePipelineCreateInfo {
    XpuDevice device;
    XpuShader compute_shader;
} XpuComputePipelineCreateInfo;

XpuResult xpuCreateComputePipeline(const XpuComputePipelineCreateInfo*, XpuPipeline* out);
void      xpuDestroyPipeline(XpuPipeline);
```

## Command buffer

```c
typedef struct XpuCommandBufferCreateInfo {
    XpuDevice              device;
    XpuCommandBufferLevel  level;  // PRIMARY or SECONDARY
} XpuCommandBufferCreateInfo;

XpuResult xpuCreateCommandBuffer(const XpuCommandBufferCreateInfo*, XpuCommandBuffer* out);
void      xpuDestroyCommandBuffer(XpuCommandBuffer);
XpuResult xpuBeginCommandBuffer(XpuCommandBuffer);
XpuResult xpuEndCommandBuffer(XpuCommandBuffer);
XpuResult xpuResetCommandBuffer(XpuCommandBuffer);

// Recording
XpuResult xpuCmdBindPipeline(XpuCommandBuffer, XpuPipeline);
XpuResult xpuCmdBindVertexBuffer(XpuCommandBuffer, uint32_t binding, XpuBuffer, xpu_size offset);
XpuResult xpuCmdBindIndexBuffer(XpuCommandBuffer, XpuBuffer, xpu_size offset, xpu_bool idx32);
XpuResult xpuCmdBindUniformBuffer(XpuCommandBuffer, uint32_t set, uint32_t binding,
                                    XpuBuffer, xpu_size offset, xpu_size size);
XpuResult xpuCmdBindStorageBuffer(XpuCommandBuffer, uint32_t set, uint32_t binding,
                                    XpuBuffer, xpu_size offset, xpu_size size);
XpuResult xpuCmdSetViewport(XpuCommandBuffer, float x, float y, float w, float h, float zn, float zf);
XpuResult xpuCmdSetScissor(XpuCommandBuffer, int32_t x, int32_t y, uint32_t w, uint32_t h);

XpuResult xpuCmdDraw(XpuCommandBuffer, uint32_t vertex_count, uint32_t instance_count,
                       uint32_t first_vertex, uint32_t first_instance);
XpuResult xpuCmdDrawIndexed(XpuCommandBuffer, uint32_t index_count, uint32_t instance_count,
                              uint32_t first_index, int32_t vertex_offset, uint32_t first_instance);
XpuResult xpuCmdDispatch(XpuCommandBuffer, uint32_t x, uint32_t y, uint32_t z);

XpuResult xpuCmdClearColorImage(XpuCommandBuffer, XpuImage, const float rgba[4]);
XpuResult xpuCmdClearDepthStencilImage(XpuCommandBuffer, XpuImage, float depth, uint8_t stencil);
XpuResult xpuCmdPipelineBarrier(XpuCommandBuffer);
```

## Queue submit

```c
typedef struct XpuSubmitInfo {
    XpuCommandBuffer* command_buffers;
    uint32_t          command_buffer_count;
} XpuSubmitInfo;

XpuResult xpuQueueSubmit(XpuQueue, const XpuSubmitInfo*);
```

## Math (SIMD-accelerated)

```c
// Vec4
XpuVec4 xpu_vec4_add(XpuVec4 a, XpuVec4 b);
XpuVec4 xpu_vec4_sub(XpuVec4 a, XpuVec4 b);
XpuVec4 xpu_vec4_mul(XpuVec4 a, XpuVec4 b);
XpuVec4 xpu_vec4_scale(XpuVec4 a, float s);
float   xpu_vec4_dot(XpuVec4 a, XpuVec4 b);
float   xpu_vec4_length(XpuVec4 a);
XpuVec4 xpu_vec4_normalize(XpuVec4 a);
XpuVec4 xpu_vec4_cross_xyz(XpuVec4 a, XpuVec4 b);
XpuVec4 xpu_vec4_lerp(XpuVec4 a, XpuVec4 b, float t);

// Mat4
XpuMat4 xpu_mat4_identity();
XpuMat4 xpu_mat4_mul(XpuMat4 a, XpuMat4 b);
XpuMat4 xpu_mat4_transpose(XpuMat4 m);
XpuMat4 xpu_mat4_inverse(XpuMat4 m);
XpuVec4 xpu_mat4_transform(XpuMat4 m, XpuVec4 v);

XpuMat4 xpu_mat4_translate(float x, float y, float z);
XpuMat4 xpu_mat4_scale(float x, float y, float z);
XpuMat4 xpu_mat4_rotate_x(float radians);
XpuMat4 xpu_mat4_rotate_y(float radians);
XpuMat4 xpu_mat4_rotate_z(float radians);
XpuMat4 xpu_mat4_rotate_axis(XpuVec3 axis, float radians);

XpuMat4 xpu_mat4_lookat(XpuVec3 eye, XpuVec3 center, XpuVec3 up);
XpuMat4 xpu_mat4_perspective(float fovy, float aspect, float near, float far);
XpuMat4 xpu_mat4_orthographic(float l, float r, float b, float t, float n, float f);

// Quaternion
XpuQuat xpu_quat_identity();
XpuQuat xpu_quat_from_axis_angle(XpuVec3 axis, float radians);
XpuQuat xpu_quat_mul(XpuQuat a, XpuQuat b);
XpuQuat xpu_quat_normalize(XpuQuat q);
XpuMat4 xpu_quat_to_mat4(XpuQuat q);

// Batched transform (used by software backend)
void xpu_transform_vertices(const XpuMat4* mvp, const XpuVec4* src,
                              XpuVec4* dst, xpu_size count);

// CPU detection
XpuCpuArch  xpu_math_detect_cpu_arch();
const char* xpu_math_arch_name(XpuCpuArch);
```

## Result codes

| Code | Meaning |
|------|---------|
| `XPU_SUCCESS` | OK |
| `XPU_ERROR_NOT_READY` | Instance/device not initialized |
| `XPU_ERROR_OUT_OF_MEMORY` | Allocation failed |
| `XPU_ERROR_INVALID_ARG` | Bad argument |
| `XPU_ERROR_DEVICE_LOST` | GPU crashed |
| `XPU_ERROR_UNSUPPORTED` | Feature not supported |
| `XPU_ERROR_FORMAT_MISMATCH` | Bad pixel format |
| `XPU_ERROR_SHADER_COMPILE` | Shader compilation failed |
| `XPU_ERROR_BACKEND` | Backend-specific failure |
| `XPU_ERROR_PLATFORM` | OS-level failure |
