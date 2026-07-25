/**
 * XPU - src/backend/backend_vulkan.cpp
 *
 * Vulkan 1.2 backend stub. On devices with Vulkan support (modern
 * Android, desktop Linux, Windows), this is the fastest backend.
 *
 * Build with -DXPU_HAS_VULKAN=1 and link against libvulkan.
 */

#include "backend.h"

#ifndef XPU_HAS_VULKAN
extern "C" int xpu_backend_vulkan_probe(xpu_backend_vtable*, void**) { return 0; }
#else

#include <vulkan/vulkan.h>
#include <cstring>

extern "C" int xpu_backend_vulkan_probe(xpu_backend_vtable* vt, void** state) {
    std::memset(vt, 0, sizeof(*vt));
    *state = nullptr;
    return 0;
}

#endif
