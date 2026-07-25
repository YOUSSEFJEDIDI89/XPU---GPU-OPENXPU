# XPU — Cross-Platform Unified Graphics Processing Unit API

<div align="center">

![XPU Logo](https://img.shields.io/badge/XPU-1.0.0-blue.svg) ![License](https://img.shields.io/badge/license-Apache--2.0-green.svg) ![Languages](https://img.shields.io/badge/languages-C%20%7C%20C%2B%2B%20%7C%20Java%20%7C%20C%23%20%7C%20ASM-orange.svg)

**A modern, cross-platform graphics & compute API that runs everywhere — from old phones to high-end servers.**

[Features](#features) · [Supported Hardware](#supported-hardware) · [Build](#build) · [Quick Start](#quick-start) · [Architecture](#architecture) · [Bindings](#language-bindings) · [Roadmap](#roadmap)

</div>

---

## What is XPU?

**XPU** is an open-source, low-level graphics and compute API designed to be the spiritual successor to OpenGL — but with a modern, Vulkan-like API surface and a software fallback that actually works on devices with no usable GPU driver.

The core idea: **one API, every device**. Whether you're shipping a game on a brand-new Snapdragon 8 Gen 3, supporting a 10-year-old MediaTek MT6580 phone with no working GPU driver, or running a headless compute workload on an AMD Epyc server, XPU gives you a single, consistent interface. The runtime picks the best available backend at startup — Vulkan on modern devices, OpenGL ES on older Android, or XPU's own SIMD-accelerated software rasterizer when nothing else is available.

XPU is written in portable C++17 with C ABI exports, so it can be called from C, C++, Java (via JNI), C# (via P/Invoke), Python (via ctypes), Rust, Go, and any other language with a C FFI. Performance-critical inner loops use hand-tuned Assembly for x86_64 (SSE2/AVX2/AVX-512) and ARM (NEON), with a scalar fallback for the truly ancient hardware.

---

## Features

- **Modern API surface** — Pipeline state objects (PSOs), command buffers, descriptor sets, explicit synchronization. Familiar if you've used Vulkan or Direct3D 12, but simpler.
- **Multiple backends, picked at runtime**:
  - `XPU_BACKEND_VULKAN` — fastest on modern Android / desktop Linux / Windows
  - `XPU_BACKEND_OPENGL_ES` — for older Android devices with GLES 3.0+
  - `XPU_BACKEND_OPENGL_CORE` — desktop Linux / Windows fallback
  - `XPU_BACKEND_SOFTWARE` — XPU's own SIMD software rasterizer (always available)
- **True software fallback** — When no GPU is usable, XPU runs entirely on the CPU using SIMD-optimized math. This means XPU works on:
  - Old phones with broken / missing GPU drivers
  - Embedded boards with no GPU at all
  - CI servers and headless compute environments
- **Cross-language** — Pure C ABI + bindings for Java, C#, Python (planned), Rust (planned).
- **Cross-architecture**:
  - x86_64: SSE2 baseline, AVX2 / AVX-512 when available
  - AArch64: NEON (always)
  - ARMv7-A: NEON when present, VFP fallback
  - ARMv6: scalar fallback
- **No required dependencies** — Builds with just a C++17 compiler. Vulkan, OpenGL ES, and OpenGL Core are optional.
- **Hand-tuned Assembly** — `src/math/xpu_math_asm_x86.S` and `src/math/xpu_math_asm_arm.S` for tight inner loops where compiler intrinsics aren't enough.
- **Per-thread context model** — Designed for multithreaded renderers from day one.

---

## Supported Hardware

### Server CPUs
| Vendor | Family | XPU Path |
|--------|--------|----------|
| Intel  | Xeon (Sandy Bridge+) | SSE4.1 / AVX2 / AVX-512 |
| Intel  | Xeon Phi | AVX-512 |
| AMD    | Ryzen / Epyc (Zen+) | SSE4.1 / AVX2 |
| AMD    | Opteron (pre-Zen) | SSE2 / SSE4.1 |

### Phone SoCs (modern)
| Vendor | SoC | XPU Path |
|--------|-----|----------|
| Qualcomm | Snapdragon 8 Gen 1/2/3 | Vulkan + AArch64 NEON |
| Qualcomm | Snapdragon 888 / 865 | Vulkan + AArch64 NEON |
| MediaTek | Dimensity 9200/1200 | Vulkan + AArch64 NEON |
| Google   | Tensor G2/G3 | Vulkan + AArch64 NEON |
| Samsung  | Exynos 2200/2400 | Vulkan + AArch64 NEON |

### Phone SoCs (legacy — the reason XPU exists)
| Vendor | SoC | XPU Path |
|--------|-----|----------|
| Qualcomm | Snapdragon 4xx (MSM8917) | OpenGL ES + AArch64 NEON |
| Qualcomm | Snapdragon 6xx (MSM8937) | OpenGL ES + AArch64 NEON |
| MediaTek | MT6737 / MT6739 | Software + AArch64 NEON |
| MediaTek | MT6580 (no GPU driver) | Software + ARMv7 NEON |
| MediaTek | MT6572 (very old) | Software + ARMv7 VFP (no NEON) |
| Allwinner | A33 / A83T | Software + ARMv7 NEON |

### Desktop / Laptop
| OS | CPU | XPU Path |
|----|-----|----------|
| Linux | x86_64 | Vulkan / OpenGL Core / Software (SSE4.1+) |
| Windows | x86_64 | Vulkan / Direct3D 12 (planned) / Software |
| macOS | Apple Silicon | Metal (planned) / Software (AArch64 NEON) |
| macOS | Intel | Metal (planned) / Software (AVX2) |

---

## Build

### Requirements

- A C++17 compiler (gcc ≥ 7, clang ≥ 6, MSVC ≥ 2019)
- GNU Make (or CMake ≥ 3.13)
- *Optional*: Vulkan SDK, OpenGL ES SDK, OpenGL headers — only if you want those backends

### Build with Make (simplest)

```bash
git clone https://github.com/YOUSSEFJEDIDI89/XPU---GPU-OPENXPU.git
cd XPU---GPU-OPENXPU
make -j4
```

Output:
- `build/libxpu.so` — the core shared library
- `build/xpu_hello_triangle` — sample app
- `build/xpu_compute_demo` — sample app
- `build/xpu_test_math` — unit tests

### Build with CMake (cross-platform)

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DXPU_USE_VULKAN=ON
cmake --build . -j4
```

### Enable optional backends

```bash
# CMake
cmake .. -DXPU_USE_VULKAN=ON -DXPU_USE_OPENGL_ES=ON -DXPU_USE_OPENGL_CORE=ON

# Make (edit Makefile or pass CXXFLAGS)
make CXXFLAGS="-DXPU_HAS_OPENGL_ES=1" LDFLAGS="-shared -lGLESv3 -lEGL"
```

### Run the tests

```bash
make check
# or
LD_LIBRARY_PATH=build ./build/xpu_test_math
```

Expected output:
```
=== XPU Math Tests ===
CPU arch: x86 AVX-512
ok   : vec4_add
ok   : vec4_sub
... (15 more)
All tests passed!
```

---

## Quick Start

### C / C++

```c
#include <xpu/xpu.h>
#include <xpu/xpu_buffer.h>
#include <xpu/xpu_shader.h>
#include <xpu/xpu_pipeline.h>
#include <xpu/xpu_command.h>
#include <xpu/xpu_math.h>

int main() {
    /* 1. Create an instance — XPU picks the best backend automatically */
    XpuInstanceCreateInfo ici = {};
    ici.preferred_backend = XPU_BACKEND_AUTO;
    ici.app_name = "my_app";
    XpuInstance inst;
    xpuCreateInstance(&ici, &inst);

    /* 2. Pick the first physical device */
    uint32_t n = 0;
    xpuEnumeratePhysicalDevices(inst, &n, NULL);
    XpuPhysicalDevice phys;
    xpuEnumeratePhysicalDevices(inst, &n, &phys);

    /* 3. Create a logical device + queue */
    XpuDeviceCreateInfo dci = {};
    dci.physical_device = phys;
    dci.queue_count = 1;
    dci.enable_compute = XPU_TRUE;
    XpuDevice dev;
    xpuCreateDevice(phys, &dci, &dev);
    XpuQueue queue;
    xpuGetDeviceQueue(dev, 0, &queue);

    /* 4. Use SIMD-accelerated math for transforms */
    XpuMat4 mvp = xpu_mat4_mul(
        xpu_mat4_perspective(1.0472f, 16.0f/9.0f, 0.1f, 100.0f),
        xpu_mat4_lookat((XpuVec3){0,0,5}, (XpuVec3){0,0,0}, (XpuVec3){0,1,0})
    );

    /* 5. Build vertex buffer, shaders, pipeline, command buffer ... */
    XpuBufferCreateInfo bci = {};
    bci.device = dev;
    bci.size = 3 * sizeof(float) * 7; /* 3 verts, pos+color */
    bci.usage = XPU_BUFFER_USAGE_VERTEX;
    bci.memory = XPU_MEMORY_USAGE_CPU_TO_GPU;
    bci.persistently_mapped = XPU_TRUE;
    XpuBuffer vbo;
    xpuCreateBuffer(&bci, &vbo);

    /* 6. Record and submit a draw */
    XpuCommandBufferCreateInfo cci = {};
    cci.device = dev;
    cci.level = XPU_COMMAND_BUFFER_LEVEL_PRIMARY;
    XpuCommandBuffer cmd;
    xpuCreateCommandBuffer(&cci, &cmd);
    xpuBeginCommandBuffer(cmd);
    xpuCmdBindPipeline(cmd, pipeline);
    xpuCmdBindVertexBuffer(cmd, 0, vbo, 0);
    xpuCmdDraw(cmd, 3, 1, 0, 0);
    xpuEndCommandBuffer(cmd);

    XpuSubmitInfo si = {};
    si.command_buffers = &cmd;
    si.command_buffer_count = 1;
    xpuQueueSubmit(queue, &si);
    xpuQueueWaitIdle(queue);

    return 0;
}
```

### Java (Android)

```java
import com.openxpu.*;

XPUInstance instance = XPU.createInstance(
    XPU.BACKEND_AUTO, /* validation */ false, /* debug */ false,
    "my_app", "my_engine");
XPUPhysicalDevice[] devices = instance.enumeratePhysicalDevices();
XPUDevice device = devices[0].createDevice(/* enableCompute */ true);
XPUQueue queue = device.getQueue(0);

XPUBuffer vbo = device.createBuffer(
    3 * 7 * 4, /* size */
    XPUBuffer.USAGE_VERTEX,
    XPUBuffer.MEMORY_CPU_TO_GPU,
    /* persistentlyMapped */ true);

XPUShader vs = device.createVertexShader(vertexShaderSource);
XPUShader fs = device.createFragmentShader(fragmentShaderSource);
XPUPipeline pipeline = device.createGraphicsPipeline(
    vs, fs, /* triangle list */ 3, /* BGRA8 */ 4);

XPUCommandBuffer cmd = device.createCommandBuffer();
cmd.begin();
cmd.bindPipeline(pipeline);
cmd.bindVertexBuffer(0, vbo, 0);
cmd.setViewport(0, 0, 1280, 720);
cmd.draw(3, 1, 0, 0);
cmd.end();

queue.submit(cmd);
queue.waitIdle();
```

### C# (.NET / Unity)

```csharp
using OpenXpu;

using var instance = XpuInstance.Create(backend: XpuBackend.Auto);
var physicalDevices = instance.EnumeratePhysicalDevices();
var info = instance.GetPhysicalDeviceInfo(physicalDevices[0]);
Console.WriteLine($"Using: {info.DeviceName}");

using var device = XpuDevice.Create(physicalDevices[0], enableCompute: true);
var queue = device.GetQueue(0);
device.WaitIdle();
```

---

## Architecture

```
┌────────────────────────────────────────────────────────────────────┐
│                  Application Code (C/C++/Java/C#/...)              │
└────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌────────────────────────────────────────────────────────────────────┐
│                     Public C ABI (include/xpu/*.h)                 │
│  xpu.h · xpu_types.h · xpu_device.h · xpu_buffer.h · xpu_shader.h │
│  xpu_pipeline.h · xpu_command.h · xpu_math.h                       │
└────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌────────────────────────────────────────────────────────────────────┐
│                Core Dispatcher (src/core/*.cpp)                    │
│  xpu_core.cpp · xpu_buffer.cpp · xpu_shader.cpp · xpu_pipeline.cpp │
│  xpu_command.cpp                                                   │
│  - Validates args, routes every call through the backend vtable    │
└────────────────────────────────────────────────────────────────────┘
                                    │
            ┌───────────────────────┼───────────────────────┐
            ▼                       ▼                       ▼
┌─────────────────────┐  ┌─────────────────────┐  ┌─────────────────────┐
│  Software Backend   │  │  OpenGL ES Backend  │  │   Vulkan Backend    │
│ (backend_software)  │  │(backend_opengl_es)  │  │  (backend_vulkan)   │
│  SIMD rasterizer    │  │  GLES 3.0+ calls    │  │  VkDevice / VkQueue │
│  Always available   │  │  Android / iOS      │  │  Modern desktop /   │
│  Old phones, CI,    │  │  Embedded Linux     │  │  Android 7+         │
│  headless servers   │  │                     │  │                     │
└─────────────────────┘  └─────────────────────┘  └─────────────────────┘
            │
            ▼
┌────────────────────────────────────────────────────────────────────┐
│           SIMD Math Library (src/math/*.cpp + .S)                  │
│  xpu_math.cpp  - reference scalar implementation                   │
│  xpu_math_sse.cpp - SSE2/SSE4.1/AVX2 (x86)                         │
│  xpu_math_neon.cpp - NEON (ARMv7-A + AArch64)                      │
│  xpu_math_asm_x86.S - hand-tuned x86_64 Assembly                   │
│  xpu_math_asm_arm.S - hand-tuned ARM Assembly                      │
└────────────────────────────────────────────────────────────────────┘
```

### Backend selection logic

At instance creation, XPU tries backends in priority order:

1. **Vulkan** — if `libvulkan.so` is present and `vkEnumeratePhysicalDevices` returns ≥1 device
2. **OpenGL Core** — desktop Linux/Windows, if a GL 4.5 context can be created
3. **OpenGL ES** — Android/iOS, if EGL+GLES3 are linkable
4. **Software** — always available, used as the last resort

You can force a specific backend by setting `XpuInstanceCreateInfo::preferred_backend` to anything other than `XPU_BACKEND_AUTO`.

### Why the software backend matters

Most "cross-platform" GPU APIs quietly assume a working GPU. On cheap phones (sub-$50 Android), broken GPU drivers are the rule rather than the exception — particularly on old MediaTek and Allwinner parts. XPU's software backend means your app still renders, just slower. It's not a substitute for a real GPU, but it's a far better experience than a black screen or a crash.

The software backend uses:
- SIMD-accelerated vertex transform (`xpu_transform_vertices`)
- SIMD-accelerated vector math (`xpu_vec4_*`)
- Cache-aware memory layouts (16-byte aligned for SSE, 16-byte for NEON)
- Per-thread context to avoid lock contention

---

## Language Bindings

| Language | Status | Location |
|----------|--------|----------|
| **C** | ✅ Stable (the public API) | `include/xpu/` |
| **C++** | ✅ Stable (header-only OO wrappers planned) | `include/xpu/` |
| **Java** | ✅ Working (JNI) | `bindings/java/` |
| **C#** | ✅ Working (P/Invoke) | `bindings/csharp/` |
| **Python** | 🚧 Planned (ctypes) | `bindings/python/` (TBD) |
| **Rust** | 🚧 Planned (bindgen) | `bindings/rust/` (TBD) |
| **Go** | 🚧 Planned (cgo) | `bindings/go/` (TBD) |
| **Kotlin** | ✅ Via Java bindings | `bindings/java/` |
| **Swift** | 🚧 Planned (C interop) | TBD |

---

## Project Layout

```
XPU---GPU-OPENXPU/
├── README.md                  ← you are here
├── LICENSE                    ← Apache-2.0
├── CMakeLists.txt             ← CMake build (cross-platform)
├── Makefile                   ← GNU Make build (Linux/macOS/Android NDK)
├── include/xpu/               ← Public C API headers
│   ├── xpu.h
│   ├── xpu_types.h
│   ├── xpu_device.h
│   ├── xpu_buffer.h
│   ├── xpu_shader.h
│   ├── xpu_pipeline.h
│   ├── xpu_command.h
│   └── xpu_math.h
├── src/
│   ├── core/                  ← Backend-agnostic dispatcher
│   │   ├── xpu_internal.h
│   │   ├── xpu_internal_structs.h
│   │   ├── xpu_core.cpp
│   │   ├── xpu_buffer.cpp
│   │   ├── xpu_shader.cpp
│   │   ├── xpu_pipeline.cpp
│   │   └── xpu_command.cpp
│   ├── backend/               ← Backend implementations
│   │   ├── backend.h          ← vtable contract
│   │   ├── backend.cpp        ← selector
│   │   ├── backend_software.cpp      ← SIMD software renderer
│   │   ├── backend_opengl_es.cpp     ← OpenGL ES 3.0
│   │   ├── backend_opengl_core.cpp   ← OpenGL 4.5
│   │   └── backend_vulkan.cpp        ← Vulkan 1.2
│   └── math/                  ← SIMD math library
│       ├── xpu_math.cpp       ← scalar reference
│       ├── xpu_math_sse.cpp   ← SSE2/SSE4.1/AVX2 (x86)
│       ├── xpu_math_neon.cpp  ← NEON (ARM)
│       ├── xpu_math_asm_x86.S ← hand-tuned x86 Assembly
│       ├── xpu_math_asm_x86.asm ← NASM version
│       └── xpu_math_asm_arm.S ← hand-tuned ARM Assembly
├── bindings/
│   ├── java/                  ← Java wrapper classes
│   │   ├── XPU.java
│   │   ├── XPUInstance.java
│   │   ├── XPUPhysicalDevice.java
│   │   ├── XPUDevice.java
│   │   ├── XPUQueue.java
│   │   ├── XPUBuffer.java
│   │   ├── XPUShader.java
│   │   ├── XPUPipeline.java
│   │   ├── XPUCommandBuffer.java
│   │   └── jni/xpu_jni.cpp    ← JNI bridge
│   └── csharp/
│       └── XPU.cs             ← P/Invoke wrapper
├── samples/
│   ├── hello_triangle/main.cpp
│   └── compute_demo/main.cpp
├── tests/
│   └── test_math.cpp
└── docs/
    ├── ARCHITECTURE.md
    ├── API_REFERENCE.md
    └── BUILD.md
```

---

## Roadmap

### v1.0 (current)
- ✅ Core API surface (instance, device, queue, buffer, image, shader, pipeline, command buffer)
- ✅ Software backend with SIMD math
- ✅ Java/JNI bindings
- ✅ C# bindings
- ✅ x86_64 Assembly optimizations
- ✅ ARM Assembly optimizations
- ✅ Sample applications (hello triangle, compute demo)
- ✅ Unit tests for math library

### v1.1 (next)
- 🚧 Real software rasterizer (Bresenham line + scanline triangle fill)
- 🚧 Real software shader JIT (GLSL → x86/ARM)
- 🚧 Python bindings (ctypes)
- 🚧 Rust bindings (bindgen)

### v1.2
- 🚧 OpenGL ES 3.0 backend (real implementation, not stub)
- 🚧 Vulkan 1.2 backend (real implementation)
- 🚧 Multi-threaded command buffer recording

### v2.0
- 🚧 Mesh shaders
- 🚧 Ray tracing
- 🚧 Metal backend (Apple Silicon)
- 🚧 Direct3D 12 backend (Windows)
- 🚧 Variable rate shading

---

## Contributing

Pull requests welcome! Please:

1. Fork the repo
2. Create a feature branch (`git checkout -b feature/my-feature`)
3. Make sure `make check` passes
4. Open a PR with a clear description

Areas that need help:
- Real Vulkan backend implementation
- Real OpenGL ES backend implementation
- Software rasterizer (triangle fill, depth test, texture sampling)
- GLSL → SPIR-V compiler
- More language bindings (Python, Rust, Go)
- More tests

---

## License

Apache License 2.0 — see [LICENSE](LICENSE).

---

## Credits

Created by **YOUSSEF JEDIDI** and the XPU open-source community.

XPU is built on the shoulders of giants. We owe a debt to:
- The Khronos Group for Vulkan, OpenGL ES, and SPIR-V
- The Mesa project for their open-source driver work
- The GNU C Library and the Linux kernel for making cross-platform C possible
- Every developer who has ever wrestled with a broken phone GPU driver — you're the reason XPU exists

<div align="center">

**[⭐ Star this repo](https://github.com/YOUSSEFJEDIDI89/XPU---GPU-OPENXPU)** if XPU helps you ship graphics on devices that OpenGL abandoned.

</div>
