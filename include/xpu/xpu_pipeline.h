/**
 * XPU - xpu_pipeline.h - Pipeline state object (PSO) APIs
 */

#ifndef XPU_PIPELINE_H
#define XPU_PIPELINE_H

#include "xpu_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct XpuVertexInputBinding {
    uint32_t binding;
    uint32_t stride;
    xpu_bool per_instance;
} XpuVertexInputBinding;

typedef struct XpuVertexInputAttribute {
    uint32_t    location;
    uint32_t    binding;
    uint32_t    offset;
    XpuFormat   format;
} XpuVertexInputAttribute;

typedef struct XpuVertexInputState {
    const XpuVertexInputBinding*   bindings;
    uint32_t                       binding_count;
    const XpuVertexInputAttribute* attributes;
    uint32_t                       attribute_count;
} XpuVertexInputState;

typedef struct XpuRasterizerState {
    XpuPolygonMode polygon_mode;
    XpuCullMode    cull_mode;
    xpu_bool       front_counter_clockwise;
    float          line_width;
    xpu_bool       depth_clamp_enable;
    xpu_bool       depth_bias_enable;
    float          depth_bias_constant;
    float          depth_bias_slope;
    float          depth_bias_clamp;
} XpuRasterizerState;

typedef struct XpuDepthStencilState {
    xpu_bool      depth_test;
    xpu_bool      depth_write;
    XpuCompareOp  depth_compare;
    xpu_bool      stencil_test;
    xpu_bool      depth_bounds_test;
} XpuDepthStencilState;

typedef struct XpuBlendAttachment {
    xpu_bool        blend_enable;
    XpuBlendFactor  src_color;
    XpuBlendFactor  dst_color;
    XpuBlendOp      color_op;
    XpuBlendFactor  src_alpha;
    XpuBlendFactor  dst_alpha;
    XpuBlendOp      alpha_op;
    uint8_t         color_write_mask;  /* 0x1=R 0x2=G 0x4=B 0x8=A */
} XpuBlendAttachment;

typedef struct XpuColorBlendState {
    const XpuBlendAttachment* attachments;
    uint32_t                  attachment_count;
    float                     blend_constants[4];
} XpuColorBlendState;

typedef struct XpuGraphicsPipelineCreateInfo {
    XpuDevice                device;
    XpuShader                vertex_shader;
    XpuShader                fragment_shader;
    XpuShader                geometry_shader;       /* optional, can be NULL handle */
    XpuShader                compute_shader;        /* optional */
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

XPU_API XpuResult xpuCreateGraphicsPipeline(const XpuGraphicsPipelineCreateInfo* pCreateInfo,
                                              XpuPipeline* pPipeline);
XPU_API void      xpuDestroyPipeline(XpuPipeline pipeline);

/* Compute pipeline - much simpler than graphics */
typedef struct XpuComputePipelineCreateInfo {
    XpuDevice device;
    XpuShader compute_shader;
} XpuComputePipelineCreateInfo;

XPU_API XpuResult xpuCreateComputePipeline(const XpuComputePipelineCreateInfo* pCreateInfo,
                                             XpuPipeline* pPipeline);

#ifdef __cplusplus
}
#endif

#endif /* XPU_PIPELINE_H */
