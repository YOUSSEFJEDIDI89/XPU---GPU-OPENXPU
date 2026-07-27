/**
 * XPU - Cross-Platform Unified Graphics Processing Unit API
 * ============================================================
 * Copyright (c) 2024 XPU Open Source Project
 * License: Apache-2.0
 *
 * xpu_types.h - Core type definitions for the XPU API.
 *
 * XPU is designed to run on:
 *   - Servers (x86_64: Intel Xeon, AMD Ryzen/Epyc)
 *   - Modern phones (ARM: Snapdragon, MediaTek Dimensity)
 *   - Legacy phones (ARMv7-A: old Snapdragon 4xx, MediaTek MT65xx)
 *   - Embedded / IoT (ARMv6, ARMv7-M)
 *
 * The API is pure C (ABI-stable) so it can be called from C, C++,
 * Java (via JNI), C# (via P/Invoke), Python (via ctypes), Rust,
 * Go, and any other language with a C FFI.
 */

#ifndef XPU_TYPES_H
#define XPU_TYPES_H

#include <stdint.h>
#include <stddef.h>

/* ------------------------------------------------------------------ */
/* Export macro - controls symbol visibility                          */
/* ------------------------------------------------------------------ */
#if defined(_WIN32) || defined(__CYGWIN__)
  #if defined(XPU_BUILD_DLL)
    #define XPU_API __declspec(dllexport)
  #else
    #define XPU_API __declspec(dllimport)
  #endif
#else
  #if __GNUC__ >= 4
    #define XPU_API __attribute__((visibility("default")))
  #else
    #define XPU_API
  #endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Versioning                                                          */
/* ------------------------------------------------------------------ */
#define XPU_MAKE_VERSION(major, minor, patch) \
    (((uint32_t)(major) << 22) | ((uint32_t)(minor) << 12) | (uint32_t)(patch))

#define XPU_VERSION_MAJOR 1
#define XPU_VERSION_MINOR 0
#define XPU_VERSION_PATCH 0
#define XPU_API_VERSION   XPU_MAKE_VERSION(XPU_VERSION_MAJOR, XPU_VERSION_MINOR, XPU_VERSION_PATCH)

/* ------------------------------------------------------------------ */
/* Basic integer / float typedefs (explicit width)                    */
/* ------------------------------------------------------------------ */
typedef int8_t          xpu_int8;
typedef int16_t         xpu_int16;
typedef int32_t         xpu_int32;
typedef int64_t         xpu_int64;
typedef uint8_t         xpu_uint8;
typedef uint16_t        xpu_uint16;
typedef uint32_t        xpu_uint32;
typedef uint64_t        xpu_uint64;
typedef size_t          xpu_size;
typedef float           xpu_float32;
typedef double          xpu_float64;

/* ------------------------------------------------------------------ */
/* Result codes                                                        */
/* ------------------------------------------------------------------ */
typedef enum XpuResult {
    XPU_SUCCESS              = 0,
    XPU_ERROR_NOT_READY      = 1,
    XPU_ERROR_OUT_OF_MEMORY  = 2,
    XPU_ERROR_INVALID_ARG    = 3,
    XPU_ERROR_DEVICE_LOST    = 4,
    XPU_ERROR_UNSUPPORTED    = 5,
    XPU_ERROR_FORMAT_MISMATCH= 6,
    XPU_ERROR_SHADER_COMPILE = 7,
    XPU_ERROR_BACKEND        = 8,
    XPU_ERROR_PLATFORM       = 9,
    XPU_ERROR_UNKNOWN        = (-1)
} XpuResult;

/* ------------------------------------------------------------------ */
/* Booleans (C99 style - keeps ABI stable across languages)           */
/* ------------------------------------------------------------------ */
typedef int32_t xpu_bool;
#define XPU_TRUE  1
#define XPU_FALSE 0

/* ------------------------------------------------------------------ */
/* Handles - opaque pointers, never dereferenced by the user          */
/* ------------------------------------------------------------------ */
typedef struct XpuInstance_T*       XpuInstance;
typedef struct XpuPhysicalDevice_T* XpuPhysicalDevice;
typedef struct XpuDevice_T*         XpuDevice;
typedef struct XpuContext_T*        XpuContext;
typedef struct XpuQueue_T*          XpuQueue;
typedef struct XpuBuffer_T*         XpuBuffer;
typedef struct XpuImage_T*          XpuImage;
typedef struct XpuShader_T*         XpuShader;
typedef struct XpuPipeline_T*       XpuPipeline;
typedef struct XpuCommandBuffer_T*  XpuCommandBuffer;
typedef struct XpuFramebuffer_T*    XpuFramebuffer;
typedef struct XpuDescriptorSet_T*  XpuDescriptorSet;
typedef struct XpuSwapchain_T*      XpuSwapchain;

/* ------------------------------------------------------------------ */
/* Device types - what kind of processor are we running on?           */
/* ------------------------------------------------------------------ */
typedef enum XpuDeviceType {
    XPU_DEVICE_TYPE_UNKNOWN     = 0,
    XPU_DEVICE_TYPE_CPU         = 1,   /* Pure software renderer */
    XPU_DEVICE_TYPE_INTEGRATED  = 2,   /* Integrated GPU (Snapdragon Adreno, Intel iGPU) */
    XPU_DEVICE_TYPE_DISCRETE    = 3,   /* Discrete GPU (AMD Radeon, NVIDIA) */
    XPU_DEVICE_TYPE_VIRTUAL     = 4,   /* Virtual/cloud GPU */
    XPU_DEVICE_TYPE_XPU_NATIVE  = 5    /* XPU software renderer with SIMD */
} XpuDeviceType;

/* CPU architecture detected at runtime - drives which SIMD path we use */
typedef enum XpuCpuArch {
    XPU_CPU_ARCH_UNKNOWN  = 0,
    XPU_CPU_ARCH_X86_SSE2 = 1,
    XPU_CPU_ARCH_X86_AVX2 = 2,
    XPU_CPU_ARCH_X86_AVX512 = 3,
    XPU_CPU_ARCH_ARM_NEON = 4,
    XPU_CPU_ARCH_ARM_NEON64 = 5,   /* ARMv8-A 64-bit NEON */
    XPU_CPU_ARCH_ARM_VFP  = 6,     /* Old ARMv7 with VFP but no NEON (legacy phones) */
    XPU_CPU_ARCH_PPC      = 7,
    XPU_CPU_ARCH_RISCV    = 8
} XpuCpuArch;

/* Vendor IDs - help the user pick the right device on multi-GPU phones */
typedef enum XpuVendor {
    XPU_VENDOR_UNKNOWN    = 0,
    XPU_VENDOR_INTEL      = 0x8086,
    XPU_VENDOR_AMD        = 0x1002,
    XPU_VENDOR_NVIDIA     = 0x10DE,
    XPU_VENDOR_QUALCOMM   = 0x5143,   /* Snapdragon Adreno */
    XPU_VENDOR_MEDIATEK   = 0x14C3,   /* Mali/PowerVR on MediaTek */
    XPU_VENDOR_ARM_MALI   = 0x13B5,
    XPU_VENDOR_IMAGINATION= 0x1A03,   /* PowerVR (old iPhones, some MediaTek) */
    XPU_VENDOR_APPLE      = 0x106B,
    XPU_VENDOR_SOFTWARE   = 0xFFFF
} XpuVendor;

/* ------------------------------------------------------------------ */
/* Format enums                                                        */
/* ------------------------------------------------------------------ */
typedef enum XpuFormat {
    XPU_FORMAT_UNDEFINED         = 0,
    /* 8-bit UNORM */
    XPU_FORMAT_R8_UNORM          = 1,
    XPU_FORMAT_RG8_UNORM         = 2,
    XPU_FORMAT_RGBA8_UNORM       = 3,
    XPU_FORMAT_BGRA8_UNORM       = 4,
    /* 8-bit sRGB */
    XPU_FORMAT_RGBA8_sRGB        = 5,
    XPU_FORMAT_BGRA8_sRGB        = 6,
    /* 16-bit float */
    XPU_FORMAT_R16_FLOAT         = 7,
    XPU_FORMAT_RGBA16_FLOAT      = 8,
    /* 32-bit float */
    XPU_FORMAT_R32_FLOAT         = 9,
    XPU_FORMAT_RG32_FLOAT        = 10,
    XPU_FORMAT_RGBA32_FLOAT      = 11,
    /* Depth / stencil */
    XPU_FORMAT_D16_UNORM         = 12,
    XPU_FORMAT_D32_FLOAT         = 13,
    XPU_FORMAT_D24_UNORM_S8_UINT = 14,
    /* 32-bit signed int */
    XPU_FORMAT_R32_SINT          = 15,
    XPU_FORMAT_RGBA32_SINT       = 16
} XpuFormat;

/* ------------------------------------------------------------------ */
/* Memory usage flags - hint to the driver where to put the buffer    */
/* ------------------------------------------------------------------ */
typedef enum XpuMemoryUsage {
    XPU_MEMORY_USAGE_DEFAULT      = 0x00000000,
    XPU_MEMORY_USAGE_GPU_ONLY     = 0x00000001, /* device-local */
    XPU_MEMORY_USAGE_CPU_TO_GPU   = 0x00000002, /* upload */
    XPU_MEMORY_USAGE_GPU_TO_CPU   = 0x00000004, /* readback */
    XPU_MEMORY_USAGE_CPU_ONLY     = 0x00000008, /* system memory */
    XPU_MEMORY_USAGE_PERSISTENT   = 0x00000010  /* mapped all the time */
} XpuMemoryUsage;

/* ------------------------------------------------------------------ */
/* Buffer usage flags                                                  */
/* ------------------------------------------------------------------ */
typedef enum XpuBufferUsage {
    XPU_BUFFER_USAGE_VERTEX       = 0x00000001,
    XPU_BUFFER_USAGE_INDEX        = 0x00000002,
    XPU_BUFFER_USAGE_UNIFORM      = 0x00000004,
    XPU_BUFFER_USAGE_STORAGE      = 0x00000008,
    XPU_BUFFER_USAGE_INDIRECT     = 0x00000010,
    XPU_BUFFER_USAGE_TRANSFER_SRC = 0x00000020,
    XPU_BUFFER_USAGE_TRANSFER_DST = 0x00000040
} XpuBufferUsage;

/* ------------------------------------------------------------------ */
/* Shader stage flags                                                  */
/* ------------------------------------------------------------------ */
typedef enum XpuShaderStage {
    XPU_SHADER_STAGE_VERTEX       = 0x00000001,
    XPU_SHADER_STAGE_TESSELLATION = 0x00000002,
    XPU_SHADER_STAGE_GEOMETRY     = 0x00000004,
    XPU_SHADER_STAGE_FRAGMENT     = 0x00000008,
    XPU_SHADER_STAGE_COMPUTE      = 0x00000010,
    XPU_SHADER_STAGE_TASK         = 0x00000020,
    XPU_SHADER_STAGE_MESH         = 0x00000040
} XpuShaderStage;

/* ------------------------------------------------------------------ */
/* Primitive topology                                                  */
/* ------------------------------------------------------------------ */
typedef enum XpuPrimitiveTopology {
    XPU_TOPOLOGY_POINT_LIST       = 0,
    XPU_TOPOLOGY_LINE_LIST        = 1,
    XPU_TOPOLOGY_LINE_STRIP       = 2,
    XPU_TOPOLOGY_TRIANGLE_LIST    = 3,
    XPU_TOPOLOGY_TRIANGLE_STRIP   = 4,
    XPU_TOPOLOGY_TRIANGLE_FAN     = 5
} XpuPrimitiveTopology;

/* ------------------------------------------------------------------ */
/* Compare / blend / polygon mode - mirrors modern APIs              */
/* ------------------------------------------------------------------ */
typedef enum XpuCompareOp {
    XPU_COMPARE_NEVER            = 0,
    XPU_COMPARE_LESS             = 1,
    XPU_COMPARE_EQUAL            = 2,
    XPU_COMPARE_LESS_OR_EQUAL    = 3,
    XPU_COMPARE_GREATER          = 4,
    XPU_COMPARE_NOT_EQUAL        = 5,
    XPU_COMPARE_GREATER_OR_EQUAL = 6,
    XPU_COMPARE_ALWAYS           = 7
} XpuCompareOp;

typedef enum XpuBlendOp {
    XPU_BLEND_OP_ADD          = 0,
    XPU_BLEND_OP_SUBTRACT     = 1,
    XPU_BLEND_OP_REVERSE_SUBTRACT = 2,
    XPU_BLEND_OP_MIN          = 3,
    XPU_BLEND_OP_MAX          = 4
} XpuBlendOp;

typedef enum XpuBlendFactor {
    XPU_BLEND_FACTOR_ZERO            = 0,
    XPU_BLEND_FACTOR_ONE             = 1,
    XPU_BLEND_FACTOR_SRC_COLOR       = 2,
    XPU_BLEND_FACTOR_ONE_MINUS_SRC   = 3,
    XPU_BLEND_FACTOR_SRC_ALPHA       = 4,
    XPU_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA = 5
} XpuBlendFactor;

typedef enum XpuPolygonMode {
    XPU_POLYGON_MODE_FILL  = 0,
    XPU_POLYGON_MODE_LINE  = 1,
    XPU_POLYGON_MODE_POINT = 2
} XpuPolygonMode;

typedef enum XpuCullMode {
    XPU_CULL_MODE_NONE       = 0,
    XPU_CULL_MODE_FRONT      = 1,
    XPU_CULL_MODE_BACK       = 2,
    XPU_CULL_MODE_FRONT_AND_BACK = 3
} XpuCullMode;

/* ------------------------------------------------------------------ */
/* Backend type - which low-level driver is XPU sitting on top of     */
/* ------------------------------------------------------------------ */
typedef enum XpuBackendType {
    XPU_BACKEND_AUTO          = 0,   /* pick best available at runtime */
    XPU_BACKEND_SOFTWARE      = 1,   /* XPU's own SIMD software rasterizer */
    XPU_BACKEND_OPENGL_ES     = 2,   /* Android / iOS / legacy */
    XPU_BACKEND_OPENGL_CORE   = 3,   /* desktop Linux/Windows */
    XPU_BACKEND_VULKAN        = 4,   /* modern desktop + Android */
    XPU_BACKEND_METAL         = 5,   /* Apple (future) */
    XPU_BACKEND_DIRECTX12     = 6    /* Windows (future) */
} XpuBackendType;

#ifdef __cplusplus
}
#endif

#endif /* XPU_TYPES_H */
