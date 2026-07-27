/**
 * XPU - src/core/xpu_shader.cpp - Shader API wrappers
 */

#include "xpu/xpu.h"
#include "xpu/xpu_shader.h"
#include "xpu_internal.h"
#include "xpu_internal_structs.h"

#include <cstring>

extern "C" {

XpuResult xpuCreateShader(const XpuShaderCreateInfo* ci, XpuShader* out) {
    if (!ci || !out || !ci->device) return XPU_ERROR_INVALID_ARG;
    *out = nullptr;
    auto inst = ci->device->instance;
    if (!inst || !inst->vtable.create_shader) return XPU_ERROR_NOT_READY;
    return inst->vtable.create_shader(ci->device->backend_device, ci, out);
}

void xpuDestroyShader(XpuShader sh) {
    if (!sh) return;
    XpuInstance inst = xpu_internal_current_instance();
    if (inst && inst->vtable.destroy_shader) {
        inst->vtable.destroy_shader(sh);
    }
}

const char* xpuGetShaderLog(XpuShader sh) {
    if (!sh) return nullptr;
    return sh->log.c_str();
}

uint32_t xpuGetShaderResourceCount(XpuShader sh) {
    (void)sh;
    return 0;
}

XpuResult xpuGetShaderResources(XpuShader sh, uint32_t max_count, XpuShaderResource* p) {
    (void)sh; (void)max_count; (void)p;
    return XPU_SUCCESS;
}

}  /* extern "C" */
