/**
 * XPU - src/core/xpu_core.cpp
 *
 * Implementation of the instance / device / context / queue APIs.
 * Backend-agnostic: routes every call to a function table that is
 * filled in by one of the backends (software, OpenGL ES, Vulkan).
 */

#include "xpu/xpu.h"
#include "xpu/xpu_device.h"
#include "xpu_internal.h"
#include "xpu_internal_structs.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <new>

#define XPU_STRINGIFY2(x) #x
#define XPU_STRINGIFY(x) XPU_STRINGIFY2(x)

static bool g_xpu_debug = false;
static XpuDebugCallback g_debug_callback = nullptr;
static void* g_debug_user = nullptr;
static thread_local XpuInstance g_current_instance = nullptr;
static thread_local XpuContext  g_current_context  = nullptr;

void xpu_internal_log(XpuResult severity, const char* fmt, ...) {
    if (!g_debug_callback) return;
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    g_debug_callback(severity, buf, g_debug_user);
}

XpuInstance xpu_internal_current_instance() { return g_current_instance; }
void xpu_internal_set_current_instance(XpuInstance i) { g_current_instance = i; }
/* XpuContext forward-declared; xpu_internal_device_from_context is
 * defined later in this file, after XpuContext_T is fully declared. */

#define XPU_LOG_INFO(...) do { if (g_xpu_debug) std::fprintf(stderr, "[XPU INFO] " __VA_ARGS__); } while(0)

/* ------------------------------------------------------------------ */
/* C API exports                                                       */
/* ------------------------------------------------------------------ */
extern "C" {

uint32_t xpuGetVersion(void) {
    return XPU_API_VERSION;
}

const char* xpuGetVersionString(void) {
    return "XPU 1.0.0 (OpenXPU)";
}

const char* xpuGetBuildInfo(void) {
    return "XPU built " __DATE__ " " __TIME__
#ifdef __GNUC__
           " gcc-" XPU_STRINGIFY(__GNUC__) "." XPU_STRINGIFY(__GNUC_MINOR__)
#endif
           ;
}

const char* xpuResultString(XpuResult r) {
    switch (r) {
        case XPU_SUCCESS:               return "XPU_SUCCESS";
        case XPU_ERROR_NOT_READY:       return "XPU_ERROR_NOT_READY";
        case XPU_ERROR_OUT_OF_MEMORY:   return "XPU_ERROR_OUT_OF_MEMORY";
        case XPU_ERROR_INVALID_ARG:     return "XPU_ERROR_INVALID_ARG";
        case XPU_ERROR_DEVICE_LOST:     return "XPU_ERROR_DEVICE_LOST";
        case XPU_ERROR_UNSUPPORTED:     return "XPU_ERROR_UNSUPPORTED";
        case XPU_ERROR_FORMAT_MISMATCH: return "XPU_ERROR_FORMAT_MISMATCH";
        case XPU_ERROR_SHADER_COMPILE:  return "XPU_ERROR_SHADER_COMPILE";
        case XPU_ERROR_BACKEND:         return "XPU_ERROR_BACKEND";
        case XPU_ERROR_PLATFORM:        return "XPU_ERROR_PLATFORM";
        default:                        return "XPU_ERROR_UNKNOWN";
    }
}

/* ------------------------------------------------------------------ */
/* Instance                                                            */
/* ------------------------------------------------------------------ */

XpuResult xpuCreateInstance(const XpuInstanceCreateInfo* info, XpuInstance* out) {
    if (!info || !out) return XPU_ERROR_INVALID_ARG;
    *out = nullptr;

    g_xpu_debug = info->enable_debug != 0;

    XpuInstance_T* inst = new (std::nothrow) XpuInstance_T();
    if (!inst) return XPU_ERROR_OUT_OF_MEMORY;
    std::memset(inst, 0, sizeof(*inst));

    XpuBackendType requested = info->preferred_backend;
    bool probed = false;
    if (requested == XPU_BACKEND_AUTO) {
        const XpuBackendType try_order[] = {
            XPU_BACKEND_VULKAN,
            XPU_BACKEND_OPENGL_CORE,
            XPU_BACKEND_OPENGL_ES,
            XPU_BACKEND_SOFTWARE
        };
        for (auto b : try_order) {
            if (xpu_backend_probe(b, &inst->vtable, &inst->backend_state)) {
                inst->active_backend = b;
                probed = true;
                break;
            }
        }
    } else {
        probed = xpu_backend_probe(requested, &inst->vtable, &inst->backend_state);
        if (probed) inst->active_backend = requested;
    }

    if (!probed) {
        delete inst;
        return XPU_ERROR_BACKEND;
    }

    XPU_LOG_INFO("XPU instance created, backend = %s\n",
                 xpu_backend_name(inst->active_backend));

    if (inst->vtable.enumerate_physical_devices) {
        uint32_t n = 0;
        inst->vtable.enumerate_physical_devices(inst->backend_state, &n, nullptr);
        if (n > 0) {
            inst->physical_devices = new (std::nothrow) XpuPhysicalDeviceInfo[n];
            if (inst->physical_devices) {
                inst->physical_device_count = n;
                inst->vtable.enumerate_physical_devices(inst->backend_state, &n, inst->physical_devices);
            }
        }
    }

    xpu_internal_set_current_instance(inst);
    *out = inst;
    return XPU_SUCCESS;
}

void xpuDestroyInstance(XpuInstance inst) {
    if (!inst) return;
    if (inst->vtable.destroy_instance) {
        inst->vtable.destroy_instance(inst->backend_state);
    }
    delete[] inst->physical_devices;
    delete inst;
}

XpuBackendType xpuGetActiveBackend(XpuInstance inst) {
    return inst ? inst->active_backend : XPU_BACKEND_SOFTWARE;
}

XpuResult xpuRegisterDebugCallback(XpuInstance inst, XpuDebugCallback cb, void* user) {
    (void)inst;
    g_debug_callback = cb;
    g_debug_user     = user;
    return XPU_SUCCESS;
}

/* ------------------------------------------------------------------ */
/* Physical devices                                                    */
/* ------------------------------------------------------------------ */
XpuResult xpuEnumeratePhysicalDevices(XpuInstance inst, uint32_t* pCount, XpuPhysicalDevice* pDevices) {
    if (!inst || !pCount) return XPU_ERROR_INVALID_ARG;
    if (!pDevices) {
        *pCount = inst->physical_device_count;
        return XPU_SUCCESS;
    }
    uint32_t to_copy = *pCount < inst->physical_device_count ? *pCount : inst->physical_device_count;
    for (uint32_t i = 0; i < to_copy; ++i) {
        pDevices[i] = reinterpret_cast<XpuPhysicalDevice>(static_cast<uintptr_t>(i + 1));
    }
    *pCount = to_copy;
    return XPU_SUCCESS;
}

XpuResult xpuGetPhysicalDeviceInfo(XpuPhysicalDevice pd, XpuPhysicalDeviceInfo* pInfo) {
    if (!pd || !pInfo) return XPU_ERROR_INVALID_ARG;
    uintptr_t idx = reinterpret_cast<uintptr_t>(pd) - 1;
    XpuInstance inst = xpu_internal_current_instance();
    if (!inst || idx >= inst->physical_device_count) return XPU_ERROR_INVALID_ARG;
    *pInfo = inst->physical_devices[idx];
    return XPU_SUCCESS;
}

/* ------------------------------------------------------------------ */
/* Logical device + queue                                              */
/* ------------------------------------------------------------------ */

XpuResult xpuCreateDevice(XpuPhysicalDevice pd, const XpuDeviceCreateInfo* info, XpuDevice* out) {
    if (!pd || !info || !out) return XPU_ERROR_INVALID_ARG;
    *out = nullptr;
    XpuInstance inst = xpu_internal_current_instance();
    if (!inst) return XPU_ERROR_NOT_READY;

    XpuDevice_T* dev = new (std::nothrow) XpuDevice_T();
    if (!dev) return XPU_ERROR_OUT_OF_MEMORY;
    dev->instance  = inst;
    dev->physical  = pd;
    dev->perf_hint = XPU_PERF_HINT_DEFAULT;

    uintptr_t idx = reinterpret_cast<uintptr_t>(pd) - 1;
    if (idx >= inst->physical_device_count) {
        delete dev;
        return XPU_ERROR_INVALID_ARG;
    }
    dev->info = inst->physical_devices[idx];

    if (inst->vtable.create_device) {
        XpuResult r = inst->vtable.create_device(inst->backend_state, idx, info, &dev->backend_device);
        if (r != XPU_SUCCESS) {
            delete dev;
            return r;
        }
    }

    *out = dev;
    return XPU_SUCCESS;
}

void xpuDestroyDevice(XpuDevice dev) {
    if (!dev) return;
    if (dev->instance && dev->instance->vtable.destroy_device) {
        dev->instance->vtable.destroy_device(dev->backend_device);
    }
    delete dev;
}

XpuResult xpuGetDeviceQueue(XpuDevice dev, uint32_t index, XpuQueue* out) {
    if (!dev || !out) return XPU_ERROR_INVALID_ARG;
    XpuQueue_T* q = new (std::nothrow) XpuQueue_T();
    if (!q) return XPU_ERROR_OUT_OF_MEMORY;
    q->device = dev;
    q->index  = index;
    if (dev->instance->vtable.get_queue) {
        dev->instance->vtable.get_queue(dev->backend_device, index, &q->backend_queue);
    }
    *out = q;
    return XPU_SUCCESS;
}

XpuResult xpuQueueWaitIdle(XpuQueue q) {
    if (!q) return XPU_ERROR_INVALID_ARG;
    if (q->device->instance->vtable.queue_wait_idle) {
        return q->device->instance->vtable.queue_wait_idle(q->backend_queue);
    }
    return XPU_SUCCESS;
}

XpuResult xpuDeviceWaitIdle(XpuDevice dev) {
    if (!dev) return XPU_ERROR_INVALID_ARG;
    if (dev->instance->vtable.device_wait_idle) {
        return dev->instance->vtable.device_wait_idle(dev->backend_device);
    }
    return XPU_SUCCESS;
}

XpuResult xpuSetDevicePerfHint(XpuDevice dev, XpuPerfHint hint) {
    if (!dev) return XPU_ERROR_INVALID_ARG;
    dev->perf_hint = hint;
    if (dev->instance->vtable.set_perf_hint) {
        return dev->instance->vtable.set_perf_hint(dev->backend_device, hint);
    }
    return XPU_SUCCESS;
}

XpuResult xpuGetDeviceMemoryProperties(XpuDevice dev, XpuMemoryProperties* p) {
    if (!dev || !p) return XPU_ERROR_INVALID_ARG;
    std::memset(p, 0, sizeof(*p));
    if (dev->instance->vtable.get_memory_props) {
        return dev->instance->vtable.get_memory_props(dev->backend_device, p);
    }
    p->device_local_bytes    = 256ull * 1024 * 1024;
    p->host_visible_bytes    = 1024ull * 1024 * 1024;
    p->max_allocation_bytes  = 512ull * 1024 * 1024;
    p->memory_type_count     = 1;
    return XPU_SUCCESS;
}

xpu_bool xpuGetFormatSupport(XpuDevice dev, XpuFormat fmt) {
    if (!dev) return XPU_FALSE;
    if (dev->instance->vtable.format_supported) {
        return dev->instance->vtable.format_supported(dev->backend_device, fmt) ? XPU_TRUE : XPU_FALSE;
    }
    switch (fmt) {
        case XPU_FORMAT_R8_UNORM:
        case XPU_FORMAT_RGBA8_UNORM:
        case XPU_FORMAT_BGRA8_UNORM:
        case XPU_FORMAT_RGBA8_sRGB:
        case XPU_FORMAT_BGRA8_sRGB:
        case XPU_FORMAT_R32_FLOAT:
        case XPU_FORMAT_RGBA32_FLOAT:
        case XPU_FORMAT_D16_UNORM:
        case XPU_FORMAT_D32_FLOAT:
            return XPU_TRUE;
        default:
            return XPU_FALSE;
    }
}

/* ------------------------------------------------------------------ */
/* Context                                                             */
/* ------------------------------------------------------------------ */

XpuResult xpuCreateContext(XpuDevice dev, XpuContext* out) {
    if (!dev || !out) return XPU_ERROR_INVALID_ARG;
    XpuContext_T* c = new (std::nothrow) XpuContext_T();
    if (!c) return XPU_ERROR_OUT_OF_MEMORY;
    c->device = dev;
    *out = c;
    return XPU_SUCCESS;
}

void xpuDestroyContext(XpuContext c) { delete c; }
XpuContext xpuGetCurrentContext(void) { return g_current_context; }
XpuResult  xpuMakeContextCurrent(XpuContext c) { g_current_context = c; return XPU_SUCCESS; }

}  /* extern "C" */

/* Defined here because XpuContext_T is only complete inside this TU */
XpuDevice xpu_internal_device_from_context(XpuContext c) { return c ? c->device : nullptr; }
