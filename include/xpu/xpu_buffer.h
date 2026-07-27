/**
 * XPU - xpu_buffer.h - Buffer & Image resource APIs
 */

#ifndef XPU_BUFFER_H
#define XPU_BUFFER_H

#include "xpu_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct XpuBufferCreateInfo {
    XpuDevice        device;
    xpu_size         size;
    uint32_t         usage;     /* bitmask of XpuBufferUsage */
    XpuMemoryUsage   memory;
    xpu_bool         persistently_mapped;
} XpuBufferCreateInfo;

XPU_API XpuResult xpuCreateBuffer(const XpuBufferCreateInfo* pCreateInfo,
                                    XpuBuffer* pBuffer);
XPU_API void      xpuDestroyBuffer(XpuBuffer buffer);

XPU_API xpu_size  xpuGetBufferSize(XpuBuffer buffer);
XPU_API XpuResult xpuMapBuffer(XpuBuffer buffer, void** ppData);
XPU_API XpuResult xpuUnmapBuffer(XpuBuffer buffer);

XPU_API XpuResult xpuUpdateBuffer(XpuBuffer buffer,
                                    xpu_size offset,
                                    xpu_size size,
                                    const void* pData);

XPU_API XpuResult xpuFlushBufferRange(XpuBuffer buffer,
                                        xpu_size offset,
                                        xpu_size size);
XPU_API XpuResult xpuInvalidateBufferRange(XpuBuffer buffer,
                                             xpu_size offset,
                                             xpu_size size);

/* ------------------------------------------------------------------ */
/* Image (texture)                                                     */
/* ------------------------------------------------------------------ */
typedef struct XpuImageCreateInfo {
    XpuDevice  device;
    XpuFormat  format;
    uint32_t   width;
    uint32_t   height;
    uint32_t   depth;
    uint32_t   mip_levels;
    uint32_t   array_layers;
    xpu_bool   storage;
    xpu_bool   sampled;
    xpu_bool   render_target;
    xpu_bool   depth_stencil;
} XpuImageCreateInfo;

XPU_API XpuResult xpuCreateImage(const XpuImageCreateInfo* pCreateInfo,
                                   XpuImage* pImage);
XPU_API void      xpuDestroyImage(XpuImage image);

XPU_API XpuResult xpuUpdateImage(XpuImage image,
                                   uint32_t mip_level,
                                   uint32_t array_layer,
                                   const void* pData,
                                   xpu_size size);

#ifdef __cplusplus
}
#endif

#endif /* XPU_BUFFER_H */
