# XPU - GNU Make build
# Use this if CMake is not available on the target system.
# Supports Linux, macOS (with clang), and Android NDK (with NDK_TOOLCHAIN).

CXX      ?= g++
CC       ?= gcc
AR       ?= ar

# Auto-detect architecture for SIMD flags
UNAME_M  := $(shell uname -m)
ifeq ($(UNAME_M),x86_64)
    SIMD_FLAGS := -msse2 -mavx2 -mfma -DXPU_USE_SIMD_VEC4 -DXPU_USE_SIMD_TRANSFORM
    SIMD_SRC   := src/math/xpu_math_sse.cpp
else ifeq ($(UNAME_M),i386)
    SIMD_FLAGS := -msse2 -DXPU_USE_SIMD_VEC4 -DXPU_USE_SIMD_TRANSFORM
    SIMD_SRC   := src/math/xpu_math_sse.cpp
else ifeq ($(UNAME_M),aarch64)
    SIMD_FLAGS := -DXPU_USE_SIMD_VEC4 -DXPU_USE_SIMD_TRANSFORM
    SIMD_SRC   := src/math/xpu_math_neon.cpp
else ifneq (,$(findstring arm,$(UNAME_M)))
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
    src/math/xpu_math.cpp \
    $(SIMD_SRC)

CORE_OBJECTS := $(patsubst src/%.cpp,$(BUILD_DIR)/%.o,$(CORE_SOURCES))

SAMPLES := $(BUILD_DIR)/xpu_hello_triangle $(BUILD_DIR)/xpu_compute_demo
TESTS   := $(BUILD_DIR)/xpu_test_math

.PHONY: all clean samples tests check install

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
	$(CXX) $(CXXFLAGS) $< -L$(BUILD_DIR) -lxpu -Wl,-rpath,$(BUILD_DIR) -o $@

$(BUILD_DIR)/xpu_compute_demo: samples/compute_demo/main.cpp $(LIB)
	@mkdir -p $(dir $@)
	@echo "[CXX] $<"
	$(CXX) $(CXXFLAGS) $< -L$(BUILD_DIR) -lxpu -Wl,-rpath,$(BUILD_DIR) -o $@

tests: $(TESTS)

$(BUILD_DIR)/xpu_test_math: tests/test_math.cpp $(LIB)
	@mkdir -p $(dir $@)
	@echo "[CXX] $<"
	$(CXX) $(CXXFLAGS) $< -L$(BUILD_DIR) -lxpu -Wl,-rpath,$(BUILD_DIR) -o $@

check: tests
	@echo "Running tests..."
	@LD_LIBRARY_PATH=$(BUILD_DIR) $(BUILD_DIR)/xpu_test_math

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

install: $(LIB)
	@mkdir -p $(DESTDIR)/usr/local/lib
	@mkdir -p $(DESTDIR)/usr/local/include
	cp $(LIB) $(DESTDIR)/usr/local/lib/
	cp -r include/xpu $(DESTDIR)/usr/local/include/
	@ldconfig $(DESTDIR)/usr/local/lib 2>/dev/null || true

clean:
	rm -rf $(BUILD_DIR)
