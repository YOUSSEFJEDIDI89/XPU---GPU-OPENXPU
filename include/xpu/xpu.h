/**
 * XPU - Cross-Platform Unified Graphics Processing Unit API
 * xpu.h - Main public API entry point.
 */

#ifndef XPU_H
#define XPU_H

#include "xpu_types.h"
#include "xpu_device.h"
#include "xpu_buffer.h"
#include "xpu_shader.h"
#include "xpu_pipeline.h"
#include "xpu_command.h"
#include "xpu_math.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Version query                                                       */
/* ------------------------------------------------------------------ */
XPU_API uint32_t xpuGetVersion(void);
XPU_API const char* xpuGetVersionString(void);
XPU_API const char* xpuGetBuildInfo(void);

/* ------------------------------------------------------------------ */
/* Instance creation / destruction                     */
/* ------------------------------------------------------------------ */
typedef struct XpuInstanceCreateInfo {
    XpuBackendType preferred_backend;
    xpu_bool       enable_validation;
    xpu_bool       enable_debug;
    const char*    app_name;
    uint32_t       app_version;
    const char*    engine_name;
    uint32_t       engine_version;
} XpuInstanceCreateInfo;

XPU_API XpuResult xpuCreateInstance(const XpuInstanceCreateInfo* pCreateInfo,
                                     XpuInstance* pInstance);
XPU_API void      xpuDestroyInstance(XpuInstance instance);

XPU_API XpuBackendType xpuGetActiveBackend(XpuInstance instance);

/* ------------------------------------------------------------------ */
/* Physical device enumeration                                         */
/* ------------------------------------------------------------------ */
typedef struct XpuPhysicalDeviceInfo {
    XpuVendor     vendor;
    XpuDeviceType device_type;
    XpuCpuArch    cpu_arch;       /* valid for XPU_DEVICE_TYPE_CPU */
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

XPU_API XpuResult xpuEnumeratePhysicalDevices(XpuInstance instance,
                                                uint32_t* pCount,
                                                XpuPhysicalDevice* pDevices);
XPU_API XpuResult xpuGetPhysicalDeviceInfo(XpuPhysicalDevice physicalDevice,
                                             XpuPhysicalDeviceInfo* pInfo);

/* ------------------------------------------------------------------ */
/* Logical device + queue                                              */
/* ------------------------------------------------------------------ */
typedef struct XpuDeviceCreateInfo {
    XpuPhysicalDevice physical_device;
    uint32_t          queue_count;
    float             queue_priorities[4];
    xpu_bool          enable_compute;
} XpuDeviceCreateInfo;

XPU_API XpuResult xpuCreateDevice(XpuPhysicalDevice physicalDevice,
                                    const XpuDeviceCreateInfo* pCreateInfo,
                                    XpuDevice* pDevice);
XPU_API void      xpuDestroyDevice(XpuDevice device);

XPU_API XpuResult xpuGetDeviceQueue(XpuDevice device, uint32_t index, XpuQueue* pQueue);
XPU_API XpuResult xpuQueueWaitIdle(XpuQueue queue);

/* ------------------------------------------------------------------ */
/* Context (per-thread state)                          */
/* ------------------------------------------------------------------ */
XPU_API XpuResult xpuCreateContext(XpuDevice device, XpuContext* pContext);
XPU_API void      xpuDestroyContext(XpuContext context);
XPU_API XpuContext xpuGetCurrentContext(void);
XPU_API XpuResult  xpuMakeContextCurrent(XpuContext context);

/* ------------------------------------------------------------------ */
/* Error / debug helpers                              */
/* ------------------------------------------------------------------ */
XPU_API const char* xpuResultString(XpuResult result);

/* Returns a human-readable name for the active backend type */
XPU_API const char* xpuBackendName(XpuBackendType type);

/* Debug callback for validation layer messages */
typedef void (*XpuDebugCallback)(XpuResult severity, const char* message, void* user_data);
XPU_API XpuResult xpuRegisterDebugCallback(XpuInstance instance,
                                             XpuDebugCallback callback,
                                             void* user_data);

#ifdef __cplusplus
}
#endif

#endif /* XPU_H */
