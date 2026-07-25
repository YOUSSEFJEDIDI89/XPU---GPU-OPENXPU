/**
 * XPU - src/core/xpu_pipeline.cpp - Pipeline API wrappers
 */

#include "xpu/xpu.h"
#include "xpu/xpu_pipeline.h"
#include "xpu_internal.h"
#include "xpu_internal_structs.h"

extern "C" {

XpuResult xpuCreateGraphicsPipeline(const XpuGraphicsPipelineCreateInfo* ci, XpuPipeline* out) {
    if (!ci || !out || !ci->device) return XPU_ERROR_INVALID_ARG;
    *out = nullptr;
    auto inst = ci->device->instance;
    if (!inst || !inst->vtable.create_graphics_pipeline) return XPU_ERROR_NOT_READY;
    return inst->vtable.create_graphics_pipeline(ci->device->backend_device, ci, out);
}

XpuResult xpuCreateComputePipeline(const XpuComputePipelineCreateInfo* ci, XpuPipeline* out) {
    if (!ci || !out || !ci->device) return XPU_ERROR_INVALID_ARG;
    *out = nullptr;
    auto inst = ci->device->instance;
    if (!inst || !inst->vtable.create_compute_pipeline) return XPU_ERROR_NOT_READY;
    return inst->vtable.create_compute_pipeline(ci->device->backend_device, ci, out);
}

void xpuDestroyPipeline(XpuPipeline p) {
    if (!p) return;
    XpuInstance inst = xpu_internal_current_instance();
    if (inst && inst->vtable.destroy_pipeline) {
        inst->vtable.destroy_pipeline(p);
    }
}

}  /* extern "C" */
