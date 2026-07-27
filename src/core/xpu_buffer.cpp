/**
 * XPU - src/core/xpu_buffer.cpp - Buffer & Image API wrappers
 *
 * Thin dispatcher: validates arguments and forwards to the active
 * backend's vtable entry.
 */

#include "xpu/xpu.h"
#include "xpu/xpu_buffer.h"
#include "xpu_internal.h"
#include "xpu_internal_structs.h"

#include <cstring>

extern "C" {

XpuResult xpuCreateBuffer(const XpuBufferCreateInfo* ci, XpuBuffer* out) {
    if (!ci || !out || !ci->device || ci->size == 0) return XPU_ERROR_INVALID_ARG;
    *out = nullptr;
    auto inst = ci->device->instance;
    if (!inst || !inst->vtable.create_buffer) return XPU_ERROR_NOT_READY;
    return inst->vtable.create_buffer(ci->device->backend_device, ci, out);
}

void xpuDestroyBuffer(XpuBuffer b) {
    if (!b) return;
    /* We need to find the instance - but the buffer doesn't store it
     * directly. Use the current instance for now (works in practice
     * because buffer destruction usually happens while the instance is
     * still bound). For a real impl, store the device pointer on the
     * buffer struct. */
    XpuInstance inst = xpu_internal_current_instance();
    if (inst && inst->vtable.destroy_buffer) {
        inst->vtable.destroy_buffer(b);
    }
}

xpu_size xpuGetBufferSize(XpuBuffer b) {
    return b ? b->size : 0;
}

XpuResult xpuMapBuffer(XpuBuffer b, void** out) {
    if (!b) return XPU_ERROR_INVALID_ARG;
    XpuInstance inst = xpu_internal_current_instance();
    if (!inst || !inst->vtable.map_buffer) return XPU_ERROR_NOT_READY;
    return inst->vtable.map_buffer(b, out);
}

XpuResult xpuUnmapBuffer(XpuBuffer b) {
    if (!b) return XPU_ERROR_INVALID_ARG;
    XpuInstance inst = xpu_internal_current_instance();
    if (!inst || !inst->vtable.unmap_buffer) return XPU_ERROR_NOT_READY;
    return inst->vtable.unmap_buffer(b);
}

XpuResult xpuUpdateBuffer(XpuBuffer b, xpu_size off, xpu_size sz, const void* data) {
    if (!b || !data) return XPU_ERROR_INVALID_ARG;
    if (off + sz > b->size) return XPU_ERROR_INVALID_ARG;
    XpuInstance inst = xpu_internal_current_instance();
    if (!inst || !inst->vtable.update_buffer) {
        /* Fallback: map + memcpy + unmap */
        void* p = nullptr;
        if (xpuMapBuffer(b, &p) != XPU_SUCCESS) return XPU_ERROR_BACKEND;
        std::memcpy((char*)p + off, data, sz);
        return xpuUnmapBuffer(b);
    }
    return inst->vtable.update_buffer(b, off, sz, data);
}

XpuResult xpuFlushBufferRange(XpuBuffer b, xpu_size, xpu_size) {
    /* No-op for the SW backend (coherent). Real backends would do
     * clflush / glFlushMappedBufferRange here. */
    (void)b;
    return XPU_SUCCESS;
}

XpuResult xpuInvalidateBufferRange(XpuBuffer b, xpu_size, xpu_size) {
    (void)b;
    return XPU_SUCCESS;
}

/* Image wrappers */
XpuResult xpuCreateImage(const XpuImageCreateInfo* ci, XpuImage* out) {
    if (!ci || !out || !ci->device) return XPU_ERROR_INVALID_ARG;
    *out = nullptr;
    auto inst = ci->device->instance;
    if (!inst || !inst->vtable.create_image) return XPU_ERROR_NOT_READY;
    return inst->vtable.create_image(ci->device->backend_device, ci, out);
}

void xpuDestroyImage(XpuImage img) {
    if (!img) return;
    XpuInstance inst = xpu_internal_current_instance();
    if (inst && inst->vtable.destroy_image) {
        inst->vtable.destroy_image(img);
    }
}

XpuResult xpuUpdateImage(XpuImage img, uint32_t, uint32_t, const void* data, xpu_size sz) {
    if (!img || !data) return XPU_ERROR_INVALID_ARG;
    if (sz > img->size_bytes) return XPU_ERROR_INVALID_ARG;
    std::memcpy(img->data, data, sz);
    return XPU_SUCCESS;
}

}  /* extern "C" */
