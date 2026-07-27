/**
 * XPU - src/core/xpu_command.cpp - Command buffer API wrappers
 */

#include "xpu/xpu.h"
#include "xpu/xpu_command.h"
#include "xpu_internal.h"
#include "xpu_internal_structs.h"

#include <cstring>

extern "C" {

XpuResult xpuCreateCommandBuffer(const XpuCommandBufferCreateInfo* ci, XpuCommandBuffer* out) {
    if (!ci || !out || !ci->device) return XPU_ERROR_INVALID_ARG;
    *out = nullptr;
    auto inst = ci->device->instance;
    if (!inst || !inst->vtable.create_command_buffer) return XPU_ERROR_NOT_READY;
    return inst->vtable.create_command_buffer(ci->device->backend_device, ci->level, out);
}

void xpuDestroyCommandBuffer(XpuCommandBuffer cmd) {
    if (!cmd) return;
    XpuInstance inst = xpu_internal_current_instance();
    if (inst && inst->vtable.destroy_command_buffer) {
        inst->vtable.destroy_command_buffer(cmd);
    }
}

XpuResult xpuBeginCommandBuffer(XpuCommandBuffer cmd) {
    if (!cmd) return XPU_ERROR_INVALID_ARG;
    XpuInstance inst = xpu_internal_current_instance();
    if (!inst || !inst->vtable.begin_command_buffer) return XPU_ERROR_NOT_READY;
    return inst->vtable.begin_command_buffer(cmd);
}

XpuResult xpuEndCommandBuffer(XpuCommandBuffer cmd) {
    if (!cmd) return XPU_ERROR_INVALID_ARG;
    XpuInstance inst = xpu_internal_current_instance();
    if (!inst || !inst->vtable.end_command_buffer) return XPU_ERROR_NOT_READY;
    return inst->vtable.end_command_buffer(cmd);
}

XpuResult xpuResetCommandBuffer(XpuCommandBuffer cmd) {
    if (!cmd) return XPU_ERROR_INVALID_ARG;
    XpuInstance inst = xpu_internal_current_instance();
    if (!inst || !inst->vtable.reset_command_buffer) return XPU_ERROR_NOT_READY;
    return inst->vtable.reset_command_buffer(cmd);
}

XpuResult xpuCmdBindPipeline(XpuCommandBuffer cmd, XpuPipeline p) {
    XpuInstance inst = xpu_internal_current_instance();
    if (!inst || !inst->vtable.cmd_bind_pipeline) return XPU_ERROR_NOT_READY;
    return inst->vtable.cmd_bind_pipeline(cmd, p);
}

XpuResult xpuCmdBindVertexBuffer(XpuCommandBuffer cmd, uint32_t binding, XpuBuffer b, xpu_size off) {
    XpuInstance inst = xpu_internal_current_instance();
    if (!inst || !inst->vtable.cmd_bind_vertex_buffer) return XPU_ERROR_NOT_READY;
    return inst->vtable.cmd_bind_vertex_buffer(cmd, binding, b, off);
}

XpuResult xpuCmdBindIndexBuffer(XpuCommandBuffer cmd, XpuBuffer b, xpu_size off, xpu_bool idx32) {
    XpuInstance inst = xpu_internal_current_instance();
    if (!inst || !inst->vtable.cmd_bind_index_buffer) return XPU_ERROR_NOT_READY;
    return inst->vtable.cmd_bind_index_buffer(cmd, b, off, idx32);
}

XpuResult xpuCmdBindUniformBuffer(XpuCommandBuffer cmd, uint32_t set, uint32_t binding, XpuBuffer b, xpu_size off, xpu_size sz) {
    XpuInstance inst = xpu_internal_current_instance();
    if (!inst || !inst->vtable.cmd_bind_uniform) return XPU_ERROR_NOT_READY;
    return inst->vtable.cmd_bind_uniform(cmd, set, binding, b, off, sz);
}

XpuResult xpuCmdBindStorageBuffer(XpuCommandBuffer cmd, uint32_t set, uint32_t binding, XpuBuffer b, xpu_size off, xpu_size sz) {
    XpuInstance inst = xpu_internal_current_instance();
    if (!inst || !inst->vtable.cmd_bind_storage) return XPU_ERROR_NOT_READY;
    return inst->vtable.cmd_bind_storage(cmd, set, binding, b, off, sz);
}

XpuResult xpuCmdSetViewport(XpuCommandBuffer cmd, float x, float y, float w, float h, float zn, float zf) {
    XpuInstance inst = xpu_internal_current_instance();
    if (!inst || !inst->vtable.cmd_set_viewport) return XPU_ERROR_NOT_READY;
    return inst->vtable.cmd_set_viewport(cmd, x, y, w, h, zn, zf);
}

XpuResult xpuCmdSetScissor(XpuCommandBuffer cmd, int32_t x, int32_t y, uint32_t w, uint32_t h) {
    XpuInstance inst = xpu_internal_current_instance();
    if (!inst || !inst->vtable.cmd_set_scissor) return XPU_ERROR_NOT_READY;
    return inst->vtable.cmd_set_scissor(cmd, x, y, w, h);
}

XpuResult xpuCmdDraw(XpuCommandBuffer cmd, uint32_t v, uint32_t i, uint32_t fv, uint32_t fi) {
    XpuInstance inst = xpu_internal_current_instance();
    if (!inst || !inst->vtable.cmd_draw) return XPU_ERROR_NOT_READY;
    return inst->vtable.cmd_draw(cmd, v, i, fv, fi);
}

XpuResult xpuCmdDrawIndexed(XpuCommandBuffer cmd, uint32_t idx, uint32_t instance_count, uint32_t fi, int32_t vo, uint32_t finst) {
    XpuInstance inst = xpu_internal_current_instance();
    if (!inst || !inst->vtable.cmd_draw_indexed) return XPU_ERROR_NOT_READY;
    return inst->vtable.cmd_draw_indexed(cmd, idx, instance_count, fi, vo, finst);
}

XpuResult xpuCmdDispatch(XpuCommandBuffer cmd, uint32_t x, uint32_t y, uint32_t z) {
    XpuInstance inst = xpu_internal_current_instance();
    if (!inst || !inst->vtable.cmd_dispatch) return XPU_ERROR_NOT_READY;
    return inst->vtable.cmd_dispatch(cmd, x, y, z);
}

XpuResult xpuCmdClearColorImage(XpuCommandBuffer cmd, XpuImage img, const float rgba[4]) {
    XpuInstance inst = xpu_internal_current_instance();
    if (!inst || !inst->vtable.cmd_clear_color) return XPU_ERROR_NOT_READY;
    return inst->vtable.cmd_clear_color(cmd, img, rgba);
}

XpuResult xpuCmdClearDepthStencilImage(XpuCommandBuffer cmd, XpuImage img, float depth, uint8_t /*stencil*/) {
    /* For the SW backend we just write depth into the first pixel; real
     * backends would clear the depth/stencil aspect. */
    (void)cmd;
    if (!img) return XPU_ERROR_INVALID_ARG;
    /* Treat depth as a packed float into a uint32 - SW-only convenience */
    uint32_t* pixels = (uint32_t*)img->data;
    if (pixels && img->size_bytes >= sizeof(uint32_t)) {
        std::memcpy(pixels, &depth, sizeof(float));
    }
    return XPU_SUCCESS;
}

XpuResult xpuCmdPipelineBarrier(XpuCommandBuffer cmd) {
    XpuInstance inst = xpu_internal_current_instance();
    if (!inst || !inst->vtable.cmd_pipeline_barrier) return XPU_ERROR_NOT_READY;
    return inst->vtable.cmd_pipeline_barrier(cmd);
}

XpuResult xpuQueueSubmit(XpuQueue q, const XpuSubmitInfo* info) {
    if (!q || !info) return XPU_ERROR_INVALID_ARG;
    auto inst = q->device->instance;
    if (!inst || !inst->vtable.queue_submit) return XPU_ERROR_NOT_READY;
    return inst->vtable.queue_submit(q->backend_queue, info);
}

}  /* extern "C" */
