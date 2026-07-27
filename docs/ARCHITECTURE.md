# XPU Architecture

This document explains the internal architecture of XPU, for contributors who want to extend it.

## Overview

XPU is structured in three layers:

1. **Public C API** — the ABI-stable interface in `include/xpu/`
2. **Core dispatcher** — backend-agnostic implementation in `src/core/`
3. **Backends** — actual GPU/CPU implementations in `src/backend/`

The dispatcher uses a **vtable pattern**: at instance creation, one of the backends is probed and its function table is stored on the instance. Every subsequent API call routes through this table.

```
Application
    ↓
xpuCreateBuffer()           ← public API in libxpu
    ↓
xpu_buffer.cpp              ← validates args
    ↓
inst->vtable.create_buffer  ← indirect call through vtable
    ↓
sw_create_buffer / gl_create_buffer / vk_create_buffer
```

## Why a vtable instead of #ifdef?

A vtable means:

- A single `libxpu.so` can support multiple backends at runtime
- The application can choose its backend at startup with no recompilation
- New backends (Metal, Direct3D 12) can be added without touching the core

## Backend interface

The contract every backend must implement is in `src/backend/backend.h`:

```c
typedef struct xpu_backend_vtable {
    XpuResult (*create_instance)(void** out_state);
    void      (*destroy_instance)(void* state);
    /* ... ~40 functions ... */
} xpu_backend_vtable;
```

Each backend exposes a `xpu_backend_<name>_probe()` function that:
1. Checks whether the platform can support this backend
2. Fills the vtable with function pointers
3. Allocates backend state

If probe returns 0, the core tries the next backend.

## Internal struct layout

The opaque handle types (`XpuInstance`, `XpuDevice`, etc.) are forward-declared in the public headers and fully defined in `src/core/xpu_internal_structs.h`. This keeps the ABI stable (the public headers don't leak struct sizes) while letting implementation files access struct members directly.

## SIMD math library

The math library has three implementations of each function:

| File | Architecture | When used |
|------|--------------|-----------|
| `xpu_math.cpp` | All (scalar) | Fallback when SIMD not available |
| `xpu_math_sse.cpp` | x86 (SSE2/SSE4.1) | All x86_64 builds |
| `xpu_math_neon.cpp` | ARM (NEON) | ARMv7-A with NEON + AArch64 |
| `xpu_math_asm_x86.S` | x86_64 | Optional, replaces intrinsic versions |
| `xpu_math_asm_arm.S` | ARM | Optional, replaces intrinsic versions |

The linker picks whichever version is in the link line. The scalar versions are guarded by `#ifndef XPU_USE_SIMD_VEC4` so they're omitted when SIMD is enabled.

## CPU detection

`xpu_math_detect_cpu_arch()` returns the runtime CPU architecture. On x86 it uses `__builtin_cpu_supports()` (gcc/clang) which calls CPUID. On ARM it's a compile-time constant based on `__ARM_NEON`.

This information is exposed through the `XpuPhysicalDeviceInfo::cpu_arch` field, so applications can branch on it if needed.

## Threading model

- Each thread has its own "current instance" and "current context" (stored in `thread_local` variables).
- Multiple threads can use XPU concurrently as long as they each have their own context.
- A single `XpuQueue` is **not** thread-safe — concurrent `xpuQueueSubmit` calls to the same queue need external synchronization.
- A single `XpuCommandBuffer` is **not** thread-safe — record from one thread only.

## Memory model

XPU exposes a Vulkan-like memory model:
- `XPU_MEMORY_USAGE_GPU_ONLY` — device-local, fastest for GPU, slow to update from CPU
- `XPU_MEMORY_USAGE_CPU_TO_GPU` — host-visible, optimized for upload (write-combined)
- `XPU_MEMORY_USAGE_GPU_TO_CPU` — host-visible, optimized for readback (write-back cache)
- `XPU_MEMORY_USAGE_CPU_ONLY` — system memory, used by the software backend
- `XPU_MEMORY_USAGE_PERSISTENT` — keeps the buffer mapped for the entire lifetime

The software backend ignores the distinction and always uses host-visible memory. Real GPU backends will respect the hints.

## Future work

The biggest gaps right now are:
1. The software backend records draws but doesn't actually rasterize
2. The OpenGL ES and Vulkan backends are stubs
3. There's no GLSL compiler — shaders are stored as source strings but not executed

Filling in (1) is the highest priority because it's the differentiator that makes XPU useful on old phones. A real software rasterizer needs:
- Vertex shader execution (either a small interpreter or a JIT)
- Triangle setup (edge functions, bounding box)
- Scanline rasterization (SIMD for 8 pixels at a time)
- Depth test (early-Z, hierarchical-Z)
- Texture sampling (mipmap filtering, anisotropic)
- Blend (per-attachment blend state)

Each of these is a meaningful chunk of work but well-documented in the literature.
