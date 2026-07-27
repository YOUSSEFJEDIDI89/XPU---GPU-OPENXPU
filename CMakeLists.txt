# XPU - Cross-Platform Unified Graphics Processing Unit API
# Top-level CMakeLists.txt
#
# Builds:
#   - libxpu.so / xpu.dll / libxpu.dylib     (core C/C++ library)
#   - libxpu_jni.so                            (JNI bindings for Java/Android)
#   - xpu_hello_triangle                       (sample app)
#   - xpu_compute_demo                         (sample app)
#   - xpu_test_math                            (unit tests)
#
# Usage:
#   mkdir build && cd build
#   cmake .. -DCMAKE_BUILD_TYPE=Release
#   cmake --build . -j

cmake_minimum_required(VERSION 3.13)
project(XPU VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# --------------------------------------------------------------------
# Options
# --------------------------------------------------------------------
option(XPU_BUILD_SAMPLES  "Build sample applications"          ON)
option(XPU_BUILD_TESTS    "Build unit tests"                   ON)
option(XPU_BUILD_JNI      "Build JNI bindings (requires JDK)"  OFF)
option(XPU_USE_VULKAN     "Enable Vulkan backend"              OFF)
option(XPU_USE_OPENGL_ES  "Enable OpenGL ES backend"           OFF)
option(XPU_USE_OPENGL_CORE "Enable desktop OpenGL backend"     OFF)
option(XPU_USE_SIMD       "Enable SIMD optimizations"          ON)

# --------------------------------------------------------------------
# Compiler flags
# --------------------------------------------------------------------
if(MSVC)
    add_compile_options(/W3 /O2)
else()
    add_compile_options(
        -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable
        -Wno-missing-field-initializers -Wno-unused-function
        -Wno-unused-but-set-variable
    )
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3 -fno-strict-aliasing)
    endif()
endif()

# SIMD detection
if(XPU_USE_SIMD)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|i386|i686")
        add_compile_options(-msse2 -march=native)
        include(CheckCXXCompilerFlag)
        check_cxx_compiler_flag("-mavx2" XPU_HAS_AVX2_FLAG)
        if(XPU_HAS_AVX2_FLAG)
            add_compile_options(-mavx2 -mfma)
        endif()
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm|aarch64")
        add_compile_options(-mfpu=neon)
    endif()
endif()

# --------------------------------------------------------------------
# Core library: libxpu
# --------------------------------------------------------------------
set(XPU_CORE_SOURCES
    src/core/xpu_core.cpp
    src/core/xpu_buffer.cpp
    src/core/xpu_shader.cpp
    src/core/xpu_pipeline.cpp
    src/core/xpu_command.cpp
    src/backend/backend.cpp
    src/backend/backend_software.cpp
    src/backend/backend_opengl_es.cpp
    src/backend/backend_opengl_core.cpp
    src/backend/backend_vulkan.cpp
    src/backend/sw_rasterizer.cpp
    src/math/xpu_math.cpp
    src/tensor/xpu_tensor.cpp
)

if(XPU_USE_SIMD)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|i386|i686")
        list(APPEND XPU_CORE_SOURCES src/math/xpu_math_sse.cpp)
        add_definitions(-DXPU_USE_SIMD_VEC4 -DXPU_USE_SIMD_TRANSFORM)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm|aarch64")
        list(APPEND XPU_CORE_SOURCES src/math/xpu_math_neon.cpp)
        add_definitions(-DXPU_USE_SIMD_VEC4 -DXPU_USE_SIMD_TRANSFORM)
    endif()
endif()

add_library(xpu SHARED
    ${XPU_CORE_SOURCES}
)

target_include_directories(xpu PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(XPU_USE_OPENGL_ES)
    target_compile_definitions(xpu PRIVATE -DXPU_HAS_OPENGL_ES=1)
    target_link_libraries(xpu PRIVATE EGL GLESv3)
endif()

if(XPU_USE_OPENGL_CORE)
    target_compile_definitions(xpu PRIVATE -DXPU_HAS_OPENGL_CORE=1)
    find_package(OpenGL REQUIRED)
    target_link_libraries(xpu PRIVATE OpenGL::GL)
endif()

if(XPU_USE_VULKAN)
    target_compile_definitions(xpu PRIVATE -DXPU_HAS_VULKAN=1)
    find_package(Vulkan REQUIRED)
    target_link_libraries(xpu PRIVATE Vulkan::Vulkan)
endif()

set_target_properties(xpu PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    CXX_VISIBILITY_PRESET hidden
    C_VISIBILITY_PRESET hidden
)

# --------------------------------------------------------------------
# JNI bindings
# --------------------------------------------------------------------
if(XPU_BUILD_JNI)
    find_package(JNI REQUIRED)
    add_library(xpu_jni SHARED
        bindings/java/jni/xpu_jni.cpp
    )
    target_include_directories(xpu_jni PRIVATE
        ${JNI_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(xpu_jni PRIVATE xpu)
endif()

# --------------------------------------------------------------------
# Samples
# --------------------------------------------------------------------
if(XPU_BUILD_SAMPLES)
    add_executable(xpu_hello_triangle samples/hello_triangle/main.cpp)
    target_link_libraries(xpu_hello_triangle PRIVATE xpu)

    add_executable(xpu_compute_demo samples/compute_demo/main.cpp)
    target_link_libraries(xpu_compute_demo PRIVATE xpu)

    add_executable(xpu_render_daemon samples/render_daemon/main.cpp)
    target_link_libraries(xpu_render_daemon PRIVATE xpu)

    add_executable(xpu_benchmark samples/benchmark/main.cpp)
    target_link_libraries(xpu_benchmark PRIVATE xpu)

    add_executable(xpu_nn_train samples/nn_train/main.cpp)
    target_link_libraries(xpu_nn_train PRIVATE xpu)
endif()

# --------------------------------------------------------------------
# Tests
# --------------------------------------------------------------------
if(XPU_BUILD_TESTS)
    enable_testing()
    add_executable(xpu_test_math tests/test_math.cpp)
    target_link_libraries(xpu_test_math PRIVATE xpu)
    add_test(NAME test_math COMMAND xpu_test_math)

    add_executable(xpu_test_rasterizer tests/test_rasterizer.cpp)
    target_link_libraries(xpu_test_rasterizer PRIVATE xpu)
    add_test(NAME test_rasterizer COMMAND xpu_test_rasterizer)

    add_executable(xpu_test_tensor tests/test_tensor.cpp)
    target_link_libraries(xpu_test_tensor PRIVATE xpu)
    add_test(NAME test_tensor COMMAND xpu_test_tensor)
endif()

# --------------------------------------------------------------------
# Install
# --------------------------------------------------------------------
include(GNUInstallDirs)
install(TARGETS xpu
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/xpu
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# --------------------------------------------------------------------
# Summary
# --------------------------------------------------------------------
message(STATUS "==================================================")
message(STATUS "XPU ${PROJECT_VERSION} build configuration:")
message(STATUS "  C++ Compiler     : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Build type       : ${CMAKE_BUILD_TYPE}")
message(STATUS "  Target processor : ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "  SIMD             : ${XPU_USE_SIMD}")
message(STATUS "  Samples          : ${XPU_BUILD_SAMPLES}")
message(STATUS "  Tests            : ${XPU_BUILD_TESTS}")
message(STATUS "  JNI bindings     : ${XPU_BUILD_JNI}")
message(STATUS "  Vulkan backend   : ${XPU_USE_VULKAN}")
message(STATUS "  OpenGL ES backend: ${XPU_USE_OPENGL_ES}")
message(STATUS "  OpenGL Core bkend: ${XPU_USE_OPENGL_CORE}")
message(STATUS "==================================================")
