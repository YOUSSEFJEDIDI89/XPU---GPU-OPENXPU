/**
 * XPU - xpu_device.h - Device-level APIs
 */

#ifndef XPU_DEVICE_H
#define XPU_DEVICE_H

#include "xpu_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Device memory properties */
typedef struct XpuMemoryProperties {
    uint64_t device_local_bytes;
    uint64_t host_visible_bytes;
    uint64_t max_allocation_bytes;
    uint32_t memory_type_count;
} XpuMemoryProperties;

XPU_API XpuResult xpuGetDeviceMemoryProperties(XpuDevice device,
                                                 XpuMemoryProperties* pProps);

/* Format support query */
XPU_API xpu_bool xpuGetFormatSupport(XpuDevice device, XpuFormat format);

/* Device-level performance hint - lets old phones fall back to software */
typedef enum XpuPerfHint {
    XPU_PERF_HINT_DEFAULT     = 0,
    XPU_PERF_HINT_BATTERY     = 1,  /* minimize power (old phones) */
    XPU_PERF_HINT_BALANCED    = 2,
    XPU_PERF_HINT_MAX_PERF    = 3,  /* servers / plugged-in */
    XPU_PERF_HINT_LOW_LATENCY = 4
} XpuPerfHint;

XPU_API XpuResult xpuSetDevicePerfHint(XpuDevice device, XpuPerfHint hint);

/* Wait for all device work to complete */
XPU_API XpuResult xpuDeviceWaitIdle(XpuDevice device);

#ifdef __cplusplus
}
#endif

#endif /* XPU_DEVICE_H */
