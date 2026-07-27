/**
 * XPU - xpu_command.h - Command buffer / queue submission APIs
 */

#ifndef XPU_COMMAND_H
#define XPU_COMMAND_H

#include "xpu_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum XpuCommandBufferLevel {
    XPU_COMMAND_BUFFER_LEVEL_PRIMARY   = 0,
    XPU_COMMAND_BUFFER_LEVEL_SECONDARY = 1
} XpuCommandBufferLevel;

typedef struct XpuCommandBufferCreateInfo {
    XpuDevice              device;
    XpuCommandBufferLevel  level;
} XpuCommandBufferCreateInfo;

XPU_API XpuResult xpuCreateCommandBuffer(const XpuCommandBufferCreateInfo* pCreateInfo,
                                           XpuCommandBuffer* pCmd);
XPU_API void      xpuDestroyCommandBuffer(XpuCommandBuffer cmd);

XPU_API XpuResult xpuBeginCommandBuffer(XpuCommandBuffer cmd);
XPU_API XpuResult xpuEndCommandBuffer(XpuCommandBuffer cmd);
XPU_API XpuResult xpuResetCommandBuffer(XpuCommandBuffer cmd);

/* Recording - draw commands */
XPU_API XpuResult xpuCmdBindPipeline(XpuCommandBuffer cmd, XpuPipeline pipeline);
XPU_API XpuResult xpuCmdBindVertexBuffer(XpuCommandBuffer cmd, uint32_t binding, XpuBuffer buffer, xpu_size offset);
XPU_API XpuResult xpuCmdBindIndexBuffer(XpuCommandBuffer cmd, XpuBuffer buffer, xpu_size offset, xpu_bool index_is_uint32);
XPU_API XpuResult xpuCmdBindUniformBuffer(XpuCommandBuffer cmd, uint32_t set, uint32_t binding, XpuBuffer buffer, xpu_size offset, xpu_size size);
XPU_API XpuResult xpuCmdSetViewport(XpuCommandBuffer cmd, float x, float y, float w, float h, float min_depth, float max_depth);
XPU_API XpuResult xpuCmdSetScissor(XpuCommandBuffer cmd, int32_t x, int32_t y, uint32_t w, uint32_t h);

XPU_API XpuResult xpuCmdDraw(XpuCommandBuffer cmd, uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance);
XPU_API XpuResult xpuCmdDrawIndexed(XpuCommandBuffer cmd, uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance);

XPU_API XpuResult xpuCmdDispatch(XpuCommandBuffer cmd, uint32_t x, uint32_t y, uint32_t z);

/* Compute shader invocation */
XPU_API XpuResult xpuCmdBindStorageBuffer(XpuCommandBuffer cmd, uint32_t set, uint32_t binding, XpuBuffer buffer, xpu_size offset, xpu_size size);

/* Clears / barriers */
XPU_API XpuResult xpuCmdClearColorImage(XpuCommandBuffer cmd, XpuImage image, const float rgba[4]);
XPU_API XpuResult xpuCmdClearDepthStencilImage(XpuCommandBuffer cmd, XpuImage image, float depth, uint8_t stencil);
XPU_API XpuResult xpuCmdPipelineBarrier(XpuCommandBuffer cmd);

/* Submit to queue */
typedef struct XpuSubmitInfo {
    XpuCommandBuffer* command_buffers;
    uint32_t          command_buffer_count;
} XpuSubmitInfo;

XPU_API XpuResult xpuQueueSubmit(XpuQueue queue,
                                   const XpuSubmitInfo* pSubmitInfo);

#ifdef __cplusplus
}
#endif

#endif /* XPU_COMMAND_H */
