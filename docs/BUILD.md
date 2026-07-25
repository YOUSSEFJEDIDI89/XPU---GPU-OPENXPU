# XPU Build Guide

## Quick start

```bash
git clone https://github.com/YOUSSEFJEDIDI89/XPU---GPU-OPENXPU.git
cd XPU---GPU-OPENXPU
make -j4
LD_LIBRARY_PATH=build ./build/xpu_hello_triangle
```

## Build options

### Make

| Variable | Default | Purpose |
|----------|---------|---------|
| `CXX` | `g++` | C++ compiler |
| `CXXFLAGS` | `-std=c++17 -O3 -Wall -Wextra ...` | C++ flags |
| `LDFLAGS` | `-shared` | Linker flags |
| `BUILD_DIR` | `build` | Output directory |

SIMD flags are auto-detected from `uname -m`. To force-disable SIMD:
```bash
make SIMD_FLAGS="" SIMD_SRC=""
```

### CMake

| Option | Default | Purpose |
|--------|---------|---------|
| `XPU_BUILD_SAMPLES` | `ON` | Build sample apps |
| `XPU_BUILD_TESTS` | `ON` | Build unit tests |
| `XPU_BUILD_JNI` | `OFF` | Build JNI bindings |
| `XPU_USE_VULKAN` | `OFF` | Enable Vulkan backend |
| `XPU_USE_OPENGL_ES` | `OFF` | Enable OpenGL ES backend |
| `XPU_USE_OPENGL_CORE` | `OFF` | Enable desktop OpenGL backend |
| `XPU_USE_SIMD` | `ON` | Enable SIMD optimizations |

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DXPU_USE_VULKAN=ON
cmake --build . -j4
```

## Platform-specific notes

### Linux (x86_64)

Works out of the box with gcc ≥ 7. The Makefile auto-detects AVX2/AVX-512.

### Linux (ARM64 / aarch64)

Works out of the box. NEON is always enabled on AArch64.

### Android (NDK)

Cross-compile with the NDK standalone toolchain:

```bash
# Set up NDK toolchain (one-time)
$NDK/build/tools/make-standalone-toolchain.sh \
    --arch=arm64 --api=24 --install-dir=/tmp/android-toolchain

# Build
export PATH=/tmp/android-toolchain/bin:$PATH
make CXX=aarch64-linux-android-g++ \
     CXXFLAGS="-std=c++17 -O3 -fPIC -Iinclude -DXPU_USE_SIMD_VEC4 -DXPU_USE_SIMD_TRANSFORM"
```

For the JNI bindings:
```bash
make CXX=aarch64-linux-android-g++ \
     CXXFLAGS="..." \
     samples tests
# Then build libxpu_jni.so separately:
$CXX -shared -I$JAVA_HOME/include -I$JAVA_HOME/include/linux \
     -Iinclude bindings/java/jni/xpu_jni.cpp \
     -Lbuild -lxpu -o build/libxpu_jni.so
```

### Windows (MSVC)

Use CMake:
```cmd
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

The Makefile is GNU Make only — use CMake on Windows.

### macOS

```bash
make CXX=clang++
# or
cmake .. -DCMAKE_BUILD_TYPE=Release
```

Apple Silicon uses the NEON path automatically.

## Enabling optional backends

### Vulkan

```bash
# Install Vulkan SDK first
cmake .. -DXPU_USE_VULKAN=ON
# or with Make:
make CXXFLAGS="-DXPU_HAS_VULKAN=1" LDLIBS="-lvulkan"
```

### OpenGL ES (Android / embedded)

```bash
cmake .. -DXPU_USE_OPENGL_ES=ON
# Make:
make CXXFLAGS="-DXPU_HAS_OPENGL_ES=1" LDLIBS="-lGLESv3 -lEGL"
```

### OpenGL Core (desktop)

```bash
cmake .. -DXPU_USE_OPENGL_CORE=ON
# Make:
make CXXFLAGS="-DXPU_HAS_OPENGL_CORE=1" LDLIBS="-lGL"
```

## Running tests

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
ok   : vec4_mul
...
All tests passed!
```

## Installing

```bash
sudo make install
# Installs to /usr/local/lib/libxpu.so and /usr/local/include/xpu/
```

Or to a custom prefix:
```bash
make install DESTDIR=/path/to/prefix
```

## Troubleshooting

### "error: 'xpu.h' file not found"

Make sure you're building from the repo root, and `-Iinclude` is in your CXXFLAGS. The headers live at `include/xpu/xpu.h`.

### "undefined reference to `xpuCreateInstance'"

You're linking the application but not `libxpu`. Add `-lxpu` and `-Lbuild` to your link line.

### "libxpu.so: cannot open shared object file"

The library is in `build/` but not in your loader path. Either:
- `export LD_LIBRARY_PATH=/path/to/build`
- Or `sudo make install` to put it in `/usr/local/lib`
- Or link with `-Wl,-rpath,/path/to/build` to bake the path into your binary

### Tests fail with "perspective_near_z"

Update to the latest version — this was a bug in an early test that checked the wrong coordinate.

### Software backend is selected even on a machine with a GPU

This is expected if XPU was built without `-DXPU_USE_VULKAN=ON` etc. The software backend is always built in; the others are opt-in.
