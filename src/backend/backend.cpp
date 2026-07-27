/**
 * XPU - src/backend/backend.cpp
 *
 * Backend selector. For each requested backend, we check whether the
 * platform can support it and return the matching vtable. The actual
 * backend implementations live in backend_software.cpp,
 * backend_opengl_es.cpp, backend_vulkan.cpp.
 */

#include "backend.h"
#include "xpu/xpu.h"
#include <cstring>

/* Each backend exposes its own probe function: */
extern "C" int xpu_backend_software_probe(xpu_backend_vtable* vt, void** state);
extern "C" int xpu_backend_opengl_es_probe(xpu_backend_vtable* vt, void** state);
extern "C" int xpu_backend_opengl_core_probe(xpu_backend_vtable* vt, void** state);
extern "C" int xpu_backend_vulkan_probe(xpu_backend_vtable* vt, void** state);

extern "C" int
xpu_backend_probe(XpuBackendType type, xpu_backend_vtable* out_vt, void** out_state) {
    if (!out_vt || !out_state) return 0;
    std::memset(out_vt, 0, sizeof(*out_vt));
    *out_state = nullptr;
    switch (type) {
        case XPU_BACKEND_SOFTWARE:
            return xpu_backend_software_probe(out_vt, out_state);
        case XPU_BACKEND_OPENGL_ES:
            return xpu_backend_opengl_es_probe(out_vt, out_state);
        case XPU_BACKEND_OPENGL_CORE:
            return xpu_backend_opengl_core_probe(out_vt, out_state);
        case XPU_BACKEND_VULKAN:
            return xpu_backend_vulkan_probe(out_vt, out_state);
        default:
            return 0;
    }
}

extern "C" const char* xpu_backend_name(XpuBackendType type) {
    switch (type) {
        case XPU_BACKEND_SOFTWARE:    return "software (XPU SIMD rasterizer)";
        case XPU_BACKEND_OPENGL_ES:   return "OpenGL ES";
        case XPU_BACKEND_OPENGL_CORE: return "OpenGL Core";
        case XPU_BACKEND_VULKAN:      return "Vulkan";
        case XPU_BACKEND_METAL:       return "Metal (not built)";
        case XPU_BACKEND_DIRECTX12:   return "Direct3D 12 (not built)";
        default:                      return "auto";
    }
}

/* Public API wrapper - thin shim around the internal xpu_backend_name */
extern "C" const char* xpuBackendName(XpuBackendType type) {
    return xpu_backend_name(type);
}
