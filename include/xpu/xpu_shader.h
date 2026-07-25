/**
 * XPU - xpu_shader.h - Shader / program APIs
 *
 * XPU shaders can be supplied as:
 *   - GLSL source string  (cross-translated by the backend)
 *   - SPIR-V bytecode     (preferred on Vulkan)
 *   - Pre-compiled XPU IL (intermediate language - textual)
 */

#ifndef XPU_SHADER_H
#define XPU_SHADER_H

#include "xpu_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum XpuShaderSourceType {
    XPU_SHADER_SOURCE_GLSL     = 0,
    XPU_SHADER_SOURCE_SPIRV    = 1,
    XPU_SHADER_SOURCE_XPU_IL   = 2,  /* XPU's own textual IL */
    XPU_SHADER_SOURCE_HLSL     = 3
} XpuShaderSourceType;

typedef struct XpuShaderCreateInfo {
    XpuDevice             device;
    XpuShaderSourceType   source_type;
    XpuShaderStage        stage;
    const char*           source;     /* GLSL/HLSL/IL source OR SPIR-V bytes */
    xpu_size              source_size;
    const char*           entry_point; /* e.g. "main" */
    const char* const*    defines;
    uint32_t              define_count;
} XpuShaderCreateInfo;

XPU_API XpuResult xpuCreateShader(const XpuShaderCreateInfo* pCreateInfo,
                                    XpuShader* pShader);
XPU_API void      xpuDestroyShader(XpuShader shader);

/* If compilation failed, retrieve the error log */
XPU_API const char* xpuGetShaderLog(XpuShader shader);

/* Reflection - query bindings inside a compiled shader */
typedef struct XpuShaderResource {
    char          name[64];
    uint32_t      set;
    uint32_t      binding;
    uint32_t      size_bytes;
    XpuShaderStage stages;
} XpuShaderResource;

XPU_API uint32_t   xpuGetShaderResourceCount(XpuShader shader);
XPU_API XpuResult  xpuGetShaderResources(XpuShader shader,
                                           uint32_t max_count,
                                           XpuShaderResource* pResources);

#ifdef __cplusplus
}
#endif

#endif /* XPU_SHADER_H */
