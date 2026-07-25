/**
 * XPU - bindings/java/jni/xpu_jni.cpp
 *
 * JNI bridge: lets the Java wrapper (com.openxpu.XPU) call into the
 * native XPU C API. Compiles into libxpu_jni.so and is loaded from Java
 * via System.loadLibrary("xpu_jni").
 *
 * On Android, this is typically packaged inside an AAR alongside
 * libxpu.so. The Android.mk / CMakeLists that builds the AAR must
 * depend on both `xpu` and `xpu_jni` targets.
 */

#include <jni.h>
#include <string>
#include <cstring>
#include <cstdlib>
#include "xpu/xpu.h"
#include "xpu/xpu_device.h"
#include "xpu/xpu_buffer.h"
#include "xpu/xpu_shader.h"
#include "xpu/xpu_pipeline.h"
#include "xpu/xpu_command.h"
#include "xpu/xpu_math.h"

/* Helper to convert Java string to C string */
static std::string jstr(JNIEnv* env, jstring s) {
    if (!s) return std::string();
    const char* c = env->GetStringUTFChars(s, nullptr);
    std::string r(c ? c : "");
    if (c) env->ReleaseStringUTFChars(s, c);
    return r;
}

/* Helper to throw a RuntimeException */
static void throw_runtime(JNIEnv* env, const char* msg) {
    jclass cls = env->FindClass("java/lang/RuntimeException");
    if (cls) env->ThrowNew(cls, msg);
}

/* ------------------------------------------------------------------ */
/* Top-level XPU class                                                 */
/* ------------------------------------------------------------------ */
extern "C" JNIEXPORT jint JNICALL
Java_com_openxpu_XPU_nativeGetVersion(JNIEnv*, jclass) {
    return (jint)xpuGetVersion();
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_openxpu_XPU_nativeGetVersionString(JNIEnv* env, jclass) {
    return env->NewStringUTF(xpuGetVersionString());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_openxpu_XPU_nativeGetBuildInfo(JNIEnv* env, jclass) {
    return env->NewStringUTF(xpuGetBuildInfo());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_openxpu_XPU_nativeResultString(JNIEnv* env, jclass, jint r) {
    return env->NewStringUTF(xpuResultString((XpuResult)r));
}

extern "C" JNIEXPORT jint JNICALL
Java_com_openxpu_XPU_nativeMathDetectCpuArch(JNIEnv*, jclass) {
    return (jint)xpu_math_detect_cpu_arch();
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_openxpu_XPU_nativeMathArchName(JNIEnv* env, jclass, jint a) {
    return env->NewStringUTF(xpu_math_arch_name((XpuCpuArch)a));
}

/* ------------------------------------------------------------------ */
/* Instance                                                            */
/* ------------------------------------------------------------------ */
extern "C" JNIEXPORT jlong JNICALL
Java_com_openxpu_XPU_nativeCreateInstance(JNIEnv* env, jclass,
                                            jint backend, jboolean validation, jboolean debug,
                                            jstring appName, jstring engineName) {
    XpuInstanceCreateInfo info{};
    info.preferred_backend = (XpuBackendType)backend;
    info.enable_validation = validation ? XPU_TRUE : XPU_FALSE;
    info.enable_debug      = debug ? XPU_TRUE : XPU_FALSE;
    std::string a = jstr(env, appName);
    std::string e = jstr(env, engineName);
    info.app_name    = a.c_str();
    info.engine_name = e.c_str();
    info.app_version = 1;
    info.engine_version = 1;
    XpuInstance inst = nullptr;
    XpuResult r = xpuCreateInstance(&info, &inst);
    if (r != XPU_SUCCESS) {
        throw_runtime(env, xpuResultString(r));
        return 0;
    }
    return (jlong)inst;
}

extern "C" JNIEXPORT void JNICALL
Java_com_openxpu_XPU_nativeDestroyInstance(JNIEnv*, jclass, jlong inst) {
    xpuDestroyInstance((XpuInstance)inst);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_openxpu_XPU_nativeGetActiveBackend(JNIEnv*, jclass, jlong inst) {
    return (jint)xpuGetActiveBackend((XpuInstance)inst);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_openxpu_XPU_nativeEnumeratePhysicalDevices(JNIEnv* env, jclass,
                                                       jlong inst, jlongArray out) {
    uint32_t count = 0;
    xpuEnumeratePhysicalDevices((XpuInstance)inst, &count, nullptr);
    if (out == nullptr) return (jint)count;
    if ((uint32_t)env->GetArrayLength(out) < count) count = (uint32_t)env->GetArrayLength(out);
    if (count == 0) return 0;
    XpuPhysicalDevice* devs = new XpuPhysicalDevice[count];
    xpuEnumeratePhysicalDevices((XpuInstance)inst, &count, devs);
    jlong* buf = env->GetLongArrayElements(out, nullptr);
    for (uint32_t i = 0; i < count; ++i) buf[i] = (jlong)devs[i];
    env->ReleaseLongArrayElements(out, buf, 0);
    delete[] devs;
    return (jint)count;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_openxpu_XPU_nativeGetPhysicalDeviceInfo(JNIEnv* env, jclass, jlong pd) {
    XpuPhysicalDeviceInfo info{};
    if (xpuGetPhysicalDeviceInfo((XpuPhysicalDevice)pd, &info) != XPU_SUCCESS) {
        throw_runtime(env, "xpuGetPhysicalDeviceInfo failed");
        return nullptr;
    }
    jclass cls = env->FindClass("com/openxpu/XPU$PhysicalDeviceInfo");
    if (!cls) return nullptr;
    jmethodID ctor = env->GetMethodID(cls, "<init>", "()V");
    jobject obj = env->NewObject(cls, ctor);
    env->SetIntField(obj, env->GetFieldID(cls, "vendor", "I"), (jint)info.vendor);
    env->SetIntField(obj, env->GetFieldID(cls, "deviceType", "I"), (jint)info.device_type);
    env->SetIntField(obj, env->GetFieldID(cls, "cpuArch", "I"), (jint)info.cpu_arch);
    env->SetObjectField(obj, env->GetFieldID(cls, "deviceName", "Ljava/lang/String;"),
                          env->NewStringUTF(info.device_name));
    env->SetObjectField(obj, env->GetFieldID(cls, "driverName", "Ljava/lang/String;"),
                          env->NewStringUTF(info.driver_name));
    env->SetIntField(obj, env->GetFieldID(cls, "driverVersion", "I"), (jint)info.driver_version);
    env->SetLongField(obj, env->GetFieldID(cls, "totalMemoryBytes", "J"), (jlong)info.total_memory_bytes);
    env->SetIntField(obj, env->GetFieldID(cls, "maxComputeUnits", "I"), (jint)info.max_compute_units);
    env->SetIntField(obj, env->GetFieldID(cls, "maxWorkgroupSize", "I"), (jint)info.max_workgroup_size);
    env->SetBooleanField(obj, env->GetFieldID(cls, "supportsCompute", "Z"),
                          info.supports_compute ? JNI_TRUE : JNI_FALSE);
    env->SetBooleanField(obj, env->GetFieldID(cls, "supportsGeometry", "Z"),
                          info.supports_geometry ? JNI_TRUE : JNI_FALSE);
    env->SetBooleanField(obj, env->GetFieldID(cls, "supportsTessellation", "Z"),
                          info.supports_tessellation ? JNI_TRUE : JNI_FALSE);
    env->SetBooleanField(obj, env->GetFieldID(cls, "supportsMeshShaders", "Z"),
                          info.supports_mesh_shaders ? JNI_TRUE : JNI_FALSE);
    return obj;
}

/* ------------------------------------------------------------------ */
/* Device + queue                                                      */
/* ------------------------------------------------------------------ */
extern "C" JNIEXPORT jlong JNICALL
Java_com_openxpu_XPU_nativeCreateDevice(JNIEnv* env, jclass, jlong pd, jboolean enableCompute) {
    XpuDeviceCreateInfo info{};
    info.physical_device = (XpuPhysicalDevice)pd;
    info.queue_count = 1;
    info.queue_priorities[0] = 1.0f;
    info.enable_compute = enableCompute ? XPU_TRUE : XPU_FALSE;
    XpuDevice dev = nullptr;
    if (xpuCreateDevice((XpuPhysicalDevice)pd, &info, &dev) != XPU_SUCCESS) {
        throw_runtime(env, "xpuCreateDevice failed");
        return 0;
    }
    return (jlong)dev;
}

extern "C" JNIEXPORT void JNICALL
Java_com_openxpu_XPU_nativeDestroyDevice(JNIEnv*, jclass, jlong dev) {
    xpuDestroyDevice((XpuDevice)dev);
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_openxpu_XPU_nativeGetDeviceQueue(JNIEnv* env, jclass, jlong dev, jint idx) {
    XpuQueue q = nullptr;
    if (xpuGetDeviceQueue((XpuDevice)dev, (uint32_t)idx, &q) != XPU_SUCCESS) {
        throw_runtime(env, "xpuGetDeviceQueue failed");
        return 0;
    }
    return (jlong)q;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_openxpu_XPU_nativeQueueWaitIdle(JNIEnv*, jclass, jlong q) {
    return (jint)xpuQueueWaitIdle((XpuQueue)q);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_openxpu_XPU_nativeDeviceWaitIdle(JNIEnv*, jclass, jlong d) {
    return (jint)xpuDeviceWaitIdle((XpuDevice)d);
}

/* ------------------------------------------------------------------ */
/* Buffer                                                              */
/* ------------------------------------------------------------------ */
extern "C" JNIEXPORT jlong JNICALL
Java_com_openxpu_XPU_nativeCreateBuffer(JNIEnv* env, jclass,
                                          jlong dev, jlong size, jint usage, jint memory,
                                          jboolean persistent) {
    XpuBufferCreateInfo info{};
    info.device = (XpuDevice)dev;
    info.size = (xpu_size)size;
    info.usage = (uint32_t)usage;
    info.memory = (XpuMemoryUsage)memory;
    info.persistently_mapped = persistent ? XPU_TRUE : XPU_FALSE;
    XpuBuffer buf = nullptr;
    if (xpuCreateBuffer(&info, &buf) != XPU_SUCCESS) {
        throw_runtime(env, "xpuCreateBuffer failed");
        return 0;
    }
    return (jlong)buf;
}

extern "C" JNIEXPORT void JNICALL
Java_com_openxpu_XPU_nativeDestroyBuffer(JNIEnv*, jclass, jlong b) {
    xpuDestroyBuffer((XpuBuffer)b);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_openxpu_XPU_nativeUpdateBuffer(JNIEnv* env, jclass, jlong b, jlong off, jbyteArray data) {
    if (!data) return XPU_ERROR_INVALID_ARG;
    jsize len = env->GetArrayLength(data);
    jbyte* bytes = env->GetByteArrayElements(data, nullptr);
    XpuResult r = xpuUpdateBuffer((XpuBuffer)b, (xpu_size)off, (xpu_size)len, bytes);
    env->ReleaseByteArrayElements(data, bytes, JNI_ABORT);
    return (jint)r;
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_openxpu_XPU_nativeGetBufferSize(JNIEnv*, jclass, jlong b) {
    return (jlong)xpuGetBufferSize((XpuBuffer)b);
}

/* ------------------------------------------------------------------ */
/* Shader                                                              */
/* ------------------------------------------------------------------ */
extern "C" JNIEXPORT jlong JNICALL
Java_com_openxpu_XPU_nativeCreateShader(JNIEnv* env, jclass,
                                          jlong dev, jint stage, jstring source, jstring entry) {
    std::string src = jstr(env, source);
    std::string ent = jstr(env, entry);
    XpuShaderCreateInfo info{};
    info.device = (XpuDevice)dev;
    info.source_type = XPU_SHADER_SOURCE_GLSL;
    info.stage = (XpuShaderStage)stage;
    info.source = src.c_str();
    info.source_size = src.size();
    info.entry_point = ent.c_str();
    XpuShader sh = nullptr;
    if (xpuCreateShader(&info, &sh) != XPU_SUCCESS) {
        throw_runtime(env, "xpuCreateShader failed");
        return 0;
    }
    return (jlong)sh;
}

extern "C" JNIEXPORT void JNICALL
Java_com_openxpu_XPU_nativeDestroyShader(JNIEnv*, jclass, jlong sh) {
    xpuDestroyShader((XpuShader)sh);
}

/* ------------------------------------------------------------------ */
/* Pipeline                                                            */
/* ------------------------------------------------------------------ */
extern "C" JNIEXPORT jlong JNICALL
Java_com_openxpu_XPU_nativeCreateGraphicsPipeline(JNIEnv* env, jclass,
                                                    jlong dev, jlong vs, jlong fs,
                                                    jint topology, jint colorFormat) {
    XpuGraphicsPipelineCreateInfo info{};
    info.device = (XpuDevice)dev;
    info.vertex_shader = (XpuShader)vs;
    info.fragment_shader = (XpuShader)fs;
    info.topology = (XpuPrimitiveTopology)topology;
    info.color_format = (XpuFormat)colorFormat;
    info.depth_format = XPU_FORMAT_D32_FLOAT;
    info.dynamic_viewport = XPU_TRUE;
    info.dynamic_scissor = XPU_TRUE;
    info.rasterizer.polygon_mode = XPU_POLYGON_MODE_FILL;
    info.rasterizer.cull_mode = XPU_CULL_MODE_BACK;
    info.rasterizer.line_width = 1.0f;
    XpuPipeline p = nullptr;
    if (xpuCreateGraphicsPipeline(&info, &p) != XPU_SUCCESS) {
        throw_runtime(env, "xpuCreateGraphicsPipeline failed");
        return 0;
    }
    return (jlong)p;
}

extern "C" JNIEXPORT void JNICALL
Java_com_openxpu_XPU_nativeDestroyPipeline(JNIEnv*, jclass, jlong p) {
    xpuDestroyPipeline((XpuPipeline)p);
}

/* ------------------------------------------------------------------ */
/* Command buffer                                                      */
/* ------------------------------------------------------------------ */
extern "C" JNIEXPORT jlong JNICALL
Java_com_openxpu_XPU_nativeCreateCommandBuffer(JNIEnv* env, jclass, jlong dev) {
    XpuCommandBufferCreateInfo info{};
    info.device = (XpuDevice)dev;
    info.level = XPU_COMMAND_BUFFER_LEVEL_PRIMARY;
    XpuCommandBuffer cmd = nullptr;
    if (xpuCreateCommandBuffer(&info, &cmd) != XPU_SUCCESS) {
        throw_runtime(env, "xpuCreateCommandBuffer failed");
        return 0;
    }
    return (jlong)cmd;
}

extern "C" JNIEXPORT void JNICALL
Java_com_openxpu_XPU_nativeDestroyCommandBuffer(JNIEnv*, jclass, jlong cmd) {
    xpuDestroyCommandBuffer((XpuCommandBuffer)cmd);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_openxpu_XPU_nativeBeginCommandBuffer(JNIEnv*, jclass, jlong cmd) {
    return (jint)xpuBeginCommandBuffer((XpuCommandBuffer)cmd);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_openxpu_XPU_nativeEndCommandBuffer(JNIEnv*, jclass, jlong cmd) {
    return (jint)xpuEndCommandBuffer((XpuCommandBuffer)cmd);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_openxpu_XPU_nativeCmdBindPipeline(JNIEnv*, jclass, jlong cmd, jlong p) {
    return (jint)xpuCmdBindPipeline((XpuCommandBuffer)cmd, (XpuPipeline)p);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_openxpu_XPU_nativeCmdBindVertexBuffer(JNIEnv*, jclass, jlong cmd, jint binding, jlong b, jlong off) {
    return (jint)xpuCmdBindVertexBuffer((XpuCommandBuffer)cmd, (uint32_t)binding, (XpuBuffer)b, (xpu_size)off);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_openxpu_XPU_nativeCmdSetViewport(JNIEnv*, jclass, jlong cmd, jfloat x, jfloat y, jfloat w, jfloat h) {
    return (jint)xpuCmdSetViewport((XpuCommandBuffer)cmd, x, y, w, h, 0.0f, 1.0f);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_openxpu_XPU_nativeCmdSetScissor(JNIEnv*, jclass, jlong cmd, jint x, jint y, jint w, jint h) {
    return (jint)xpuCmdSetScissor((XpuCommandBuffer)cmd, x, y, (uint32_t)w, (uint32_t)h);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_openxpu_XPU_nativeCmdDraw(JNIEnv*, jclass, jlong cmd, jint v, jint i, jint fv, jint fi) {
    return (jint)xpuCmdDraw((XpuCommandBuffer)cmd, (uint32_t)v, (uint32_t)i, (uint32_t)fv, (uint32_t)fi);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_openxpu_XPU_nativeQueueSubmit(JNIEnv* env, jclass, jlong queue, jlongArray cmds) {
    if (!cmds) return XPU_ERROR_INVALID_ARG;
    jsize n = env->GetArrayLength(cmds);
    if (n == 0) return XPU_SUCCESS;
    jlong* buf = env->GetLongArrayElements(cmds, nullptr);
    XpuCommandBuffer* native_cmds = new XpuCommandBuffer[n];
    for (jsize i = 0; i < n; ++i) native_cmds[i] = (XpuCommandBuffer)buf[i];
    XpuSubmitInfo info{};
    info.command_buffers = native_cmds;
    info.command_buffer_count = (uint32_t)n;
    XpuResult r = xpuQueueSubmit((XpuQueue)queue, &info);
    env->ReleaseLongArrayElements(cmds, buf, JNI_ABORT);
    delete[] native_cmds;
    return (jint)r;
}
