/**
 * XPU - src/core/xpu_internal.h - Internal helpers (not public API)
 */

#ifndef XPU_INTERNAL_H
#define XPU_INTERNAL_H

#include "xpu/xpu.h"

#ifdef __cplusplus
extern "C" {
#endif

XpuInstance xpu_internal_current_instance(void);
void        xpu_internal_set_current_instance(XpuInstance);
XpuDevice   xpu_internal_device_from_context(XpuContext);
void        xpu_internal_log(XpuResult severity, const char* fmt, ...);

#ifdef __cplusplus
}
#endif

#endif
