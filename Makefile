# XPU - GNU Make build
# Use this if CMake is not available on the target system.
# Supports Linux, macOS (with clang), Android NDK, and Termux.

CXX      ?= g++
CC       ?= gcc
AR       ?= ar

# Detect platform (Termux, Android NDK, native Linux)
UNAME_S  := $(shell uname -s)
UNAME_M  := $(shell uname -m)

# Are we on Termux? (Android's terminal emulator)
ifeq ($(UNAME_S),Linux)
  ifneq (,$(wildcard /data/data/com.termux))
    ON_TERMUX := 1
  endif
endif

# Auto-detect architecture for SIMD flags
ifeq ($(UNAME_M),x86_64)
    SIMD_FLAGS := -msse2 -mavx2 -mfma -DXPU_USE_SIMD_VEC4 -DXPU_USE_SIMD_TRANSFORM
    SIMD_SRC   := src/math/xpu_math_sse.cpp
else ifeq ($(UNAME_M),i386)
    SIMD_FLAGS := -msse2 -DXPU_USE_SIMD_VEC4 -DXPU_USE_SIMD_TRANSFORM
    SIMD_SRC   := src/math/xpu_math_sse.cpp
else ifeq ($(UNAME_M),aarch64)
    # AArch64 (modern phones, Apple Silicon, modern Pi)
    SIMD_FLAGS := -DXPU_USE_SIMD_VEC4 -DXPU_USE_SIMD_TRANSFORM
    SIMD_SRC   := src/math/xpu_math_neon.cpp
else ifneq (,$(findstring arm,$(UNAME_M)))
    # ARMv7 with NEON (older Android phones, Pi 2/3)
    SIMD_FLAGS := -mfpu=neon -DXPU_USE_SIMD_VEC4 -DXPU_USE_SIMD_TRANSFORM
    SIMD_SRC   := src/math/xpu_math_neon.cpp
else ifeq ($(UNAME_M),armv8l)
    # 32-bit userspace on 64-bit ARM (rare, but Termux sometimes reports this)
    SIMD_FLAGS := -mfpu=neon -DXPU_USE_SIMD_VEC4 -DXPU_USE_SIMD_TRANSFORM
    SIMD_SRC   := src/math/xpu_math_neon.cpp
else
    SIMD_FLAGS :=
    SIMD_SRC   :=
endif

CXXFLAGS ?= -std=c++17 -O3 -Wall -Wextra -Wno-unused-parameter \
	    -Wno-unused-variable -Wno-missing-field-initializers \
	    -Wno-unused-function -fno-strict-aliasing -fPIC \
	    -Iinclude $(SIMD_FLAGS)
LDFLAGS  ?= -shared
LDLIBS   ?=

BUILD_DIR := build
LIB       := $(BUILD_DIR)/libxpu.so
RPATH     := -Wl,-rpath,$(abspath $(BUILD_DIR))

CORE_SOURCES := \
    src/core/xpu_core.cpp \
    src/core/xpu_buffer.cpp \
    src/core/xpu_shader.cpp \
    src/core/xpu_pipeline.cpp \
    src/core/xpu_command.cpp \
    src/backend/backend.cpp \
    src/backend/backend_software.cpp \
    src/backend/backend_opengl_es.cpp \
    src/backend/backend_opengl_core.cpp \
    src/backend/backend_vulkan.cpp \
    src/backend/sw_rasterizer.cpp \
    src/math/xpu_math.cpp \
    src/tensor/xpu_tensor.cpp \
    $(SIMD_SRC)

CORE_OBJECTS := $(patsubst src/%.cpp,$(BUILD_DIR)/%.o,$(CORE_SOURCES))

SAMPLES := $(BUILD_DIR)/xpu_hello_triangle $(BUILD_DIR)/xpu_compute_demo $(BUILD_DIR)/xpu_render_daemon $(BUILD_DIR)/xpu_benchmark $(BUILD_DIR)/xpu_nn_train
TESTS   := $(BUILD_DIR)/xpu_test_math $(BUILD_DIR)/xpu_test_rasterizer $(BUILD_DIR)/xpu_test_tensor

.PHONY: all clean samples tests check install daemon termux benchmark

all: $(LIB) samples tests

$(LIB): $(CORE_OBJECTS) | $(BUILD_DIR)
	@echo "[LD] $@"
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD_DIR)/%.o: src/%.cpp | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	@echo "[CXX] $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

samples: $(SAMPLES)

$(BUILD_DIR)/xpu_hello_triangle: samples/hello_triangle/main.cpp $(LIB)
	@mkdir -p $(dir $@)
	@echo "[CXX] $<"
	$(CXX) $(CXXFLAGS) $< -L$(BUILD_DIR) -lxpu $(RPATH) -o $@

$(BUILD_DIR)/xpu_compute_demo: samples/compute_demo/main.cpp $(LIB)
	@mkdir -p $(dir $@)
	@echo "[CXX] $<"
	$(CXX) $(CXXFLAGS) $< -L$(BUILD_DIR) -lxpu $(RPATH) -o $@

# Render daemon links the software rasterizer directly (not through libxpu)
# because the rasterizer is currently a header-only component used by the daemon.
# For the daemon we need sw_rasterizer.o as well.
$(BUILD_DIR)/xpu_render_daemon: samples/render_daemon/main.cpp $(BUILD_DIR)/backend/sw_rasterizer.o $(LIB)
	@mkdir -p $(dir $@)
	@echo "[CXX] $<"
	$(CXX) $(CXXFLAGS) $< $(BUILD_DIR)/backend/sw_rasterizer.o \
	        -L$(BUILD_DIR) -lxpu $(RPATH) -o $@

# Neural network training demo
$(BUILD_DIR)/xpu_nn_train: samples/nn_train/main.cpp $(LIB)
	@mkdir -p $(dir $@)
	@echo "[CXX] $<"
	$(CXX) $(CXXFLAGS) $< -L$(BUILD_DIR) -lxpu $(RPATH) -o $@

# Convenience target to launch the daemon in the background
daemon: $(BUILD_DIR)/xpu_render_daemon
	@echo "Starting render daemon in background..."
	@nohup $(BUILD_DIR)/xpu_render_daemon --width 640 --height 480 > render_daemon.log 2>&1 &
	@echo "Daemon started. PID: $$!"
	@echo "Log: render_daemon.log"
	@echo "Output: render_output/"
	@echo "Stop with: kill $$!"

# Benchmark target - measures rendering throughput
$(BUILD_DIR)/xpu_benchmark: samples/benchmark/main.cpp $(BUILD_DIR)/backend/sw_rasterizer.o $(LIB)
	@mkdir -p $(dir $@)
	@echo "[CXX] $<"
	$(CXX) $(CXXFLAGS) $< $(BUILD_DIR)/backend/sw_rasterizer.o \
	        -L$(BUILD_DIR) -lxpu $(RPATH) -o $@

benchmark: $(BUILD_DIR)/xpu_benchmark
	@echo "Running XPU benchmark..."
	@LD_LIBRARY_PATH=$(BUILD_DIR) $(BUILD_DIR)/xpu_benchmark --quick

# Termux-specific target: uses clang++, fixes locale, installs deps
termux:
	@export LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8
	@command -v clang++ >/dev/null 2>&1 || pkg install -y clang make
	@$(MAKE) CXX=clang++ CC=clang
	@LD_LIBRARY_PATH=$(BUILD_DIR) ./build/xpu_test_math
	@LD_LIBRARY_PATH=$(BUILD_DIR) ./build/xpu_test_rasterizer
	@LD_LIBRARY_PATH=$(BUILD_DIR) ./build/xpu_test_tensor
	@echo ""
	@echo "Termux build complete! Run:"
	@echo "  LD_LIBRARY_PATH=build ./build/xpu_render_daemon --width 640 --height 480"
	@echo "  LD_LIBRARY_PATH=build ./build/xpu_benchmark --quick"
	@echo "  LD_LIBRARY_PATH=build ./build/xpu_nn_train"

tests: $(TESTS)

$(BUILD_DIR)/xpu_test_math: tests/test_math.cpp $(LIB)
	@mkdir -p $(dir $@)
	@echo "[CXX] $<"
	$(CXX) $(CXXFLAGS) $< -L$(BUILD_DIR) -lxpu $(RPATH) -o $@

$(BUILD_DIR)/xpu_test_rasterizer: tests/test_rasterizer.cpp $(BUILD_DIR)/backend/sw_rasterizer.o $(LIB)
	@mkdir -p $(dir $@)
	@echo "[CXX] $<"
	$(CXX) $(CXXFLAGS) $< $(BUILD_DIR)/backend/sw_rasterizer.o -L$(BUILD_DIR) -lxpu $(RPATH) -o $@

$(BUILD_DIR)/xpu_test_tensor: tests/test_tensor.cpp $(LIB)
	@mkdir -p $(dir $@)
	@echo "[CXX] $<"
	$(CXX) $(CXXFLAGS) $< -L$(BUILD_DIR) -lxpu $(RPATH) -o $@

check: tests
	@echo "Running math tests..."
	@LD_LIBRARY_PATH=$(BUILD_DIR) $(BUILD_DIR)/xpu_test_math
	@echo "Running rasterizer tests..."
	@LD_LIBRARY_PATH=$(BUILD_DIR) $(BUILD_DIR)/xpu_test_rasterizer
	@echo "Running tensor tests..."
	@LD_LIBRARY_PATH=$(BUILD_DIR) $(BUILD_DIR)/xpu_test_tensor

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

install: $(LIB)
	@mkdir -p $(DESTDIR)/usr/local/lib
	@mkdir -p $(DESTDIR)/usr/local/include
	cp $(LIB) $(DESTDIR)/usr/local/lib/
	cp -r include/xpu $(DESTDIR)/usr/local/include/
	@ldconfig $(DESTDIR)/usr/local/lib 2>/dev/null || true

clean:
	rm -rf $(BUILD_DIR) render_output render_daemon.log
