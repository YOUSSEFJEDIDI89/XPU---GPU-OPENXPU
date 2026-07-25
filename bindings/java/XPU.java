// XPU - bindings/java/XPU.java
//
// Java wrapper around the XPU C API. Uses JNI to call into libxpu.so.
// Designed to be usable from Android (Java/Kotlin) and from desktop Java.
//
// Usage:
//   XPUInstance instance = XPU.createInstance(...);
//   XPUPhysicalDevice[] devices = instance.enumeratePhysicalDevices();
//   XPUDevice device = devices[0].createDevice(...);
//   ...

package com.openxpu;

public final class XPU {
    static {
        // Load the native library. On Android this is "libxpu.so".
        // On desktop it can be "xpu" (libxpu.so / xpu.dll / libxpu.dylib).
        try {
            System.loadLibrary("xpu");
        } catch (UnsatisfiedLinkError e) {
            System.err.println("[XPU] Failed to load native library: " + e.getMessage());
            throw e;
        }
    }

    public static final int BACKEND_AUTO         = 0;
    public static final int BACKEND_SOFTWARE     = 1;
    public static final int BACKEND_OPENGL_ES    = 2;
    public static final int BACKEND_OPENGL_CORE  = 3;
    public static final int BACKEND_VULKAN       = 4;
    public static final int BACKEND_METAL        = 5;
    public static final int BACKEND_DIRECTX12    = 6;

    public static final int SUCCESS                   = 0;
    public static final int ERROR_NOT_READY           = 1;
    public static final int ERROR_OUT_OF_MEMORY       = 2;
    public static final int ERROR_INVALID_ARG         = 3;
    public static final int ERROR_DEVICE_LOST         = 4;
    public static final int ERROR_UNSUPPORTED         = 5;
    public static final int ERROR_FORMAT_MISMATCH     = 6;
    public static final int ERROR_SHADER_COMPILE      = 7;
    public static final int ERROR_BACKEND             = 8;
    public static final int ERROR_PLATFORM            = 9;

    public static final int DEVICE_TYPE_UNKNOWN     = 0;
    public static final int DEVICE_TYPE_CPU         = 1;
    public static final int DEVICE_TYPE_INTEGRATED  = 2;
    public static final int DEVICE_TYPE_DISCRETE    = 3;
    public static final int DEVICE_TYPE_VIRTUAL     = 4;
    public static final int DEVICE_TYPE_XPU_NATIVE  = 5;

    public static final int CPU_ARCH_UNKNOWN    = 0;
    public static final int CPU_ARCH_X86_SSE2   = 1;
    public static final int CPU_ARCH_X86_AVX2   = 2;
    public static final int CPU_ARCH_X86_AVX512 = 3;
    public static final int CPU_ARCH_ARM_NEON   = 4;
    public static final int CPU_ARCH_ARM_NEON64 = 5;
    public static final int CPU_ARCH_ARM_VFP    = 6;

    private XPU() {}

    /* Native API - implemented in bindings/java/jni/xpu_jni.cpp */
    public static native int    nativeGetVersion();
    public static native String nativeGetVersionString();
    public static native String nativeGetBuildInfo();
    public static native String nativeResultString(int result);
    public static native int    nativeMathDetectCpuArch();
    public static native String nativeMathArchName(int arch);

    public static native long   nativeCreateInstance(int backend, boolean validation, boolean debug,
                                                       String appName, String engineName);
    public static native void   nativeDestroyInstance(long instance);
    public static native int    nativeGetActiveBackend(long instance);
    public static native int    nativeEnumeratePhysicalDevices(long instance, long[] outHandles);
    public static native PhysicalDeviceInfo nativeGetPhysicalDeviceInfo(long physicalDevice);

    public static native long   nativeCreateDevice(long physicalDevice, boolean enableCompute);
    public static native void   nativeDestroyDevice(long device);
    public static native long   nativeGetDeviceQueue(long device, int index);
    public static native int    nativeQueueWaitIdle(long queue);
    public static native int    nativeDeviceWaitIdle(long device);

    public static native long   nativeCreateBuffer(long device, long size, int usage, int memory,
                                                     boolean persistentlyMapped);
    public static native void   nativeDestroyBuffer(long buffer);
    public static native int    nativeUpdateBuffer(long buffer, long offset, byte[] data);
    public static native long   nativeGetBufferSize(long buffer);

    public static native long   nativeCreateShader(long device, int stage, String source, String entry);
    public static native void   nativeDestroyShader(long shader);

    public static native long   nativeCreateGraphicsPipeline(long device,
                                                               long vertexShader,
                                                               long fragmentShader,
                                                               int topology,
                                                               int colorFormat);
    public static native void   nativeDestroyPipeline(long pipeline);

    public static native long   nativeCreateCommandBuffer(long device);
    public static native void   nativeDestroyCommandBuffer(long cmd);
    public static native int    nativeBeginCommandBuffer(long cmd);
    public static native int    nativeEndCommandBuffer(long cmd);
    public static native int    nativeCmdBindPipeline(long cmd, long pipeline);
    public static native int    nativeCmdBindVertexBuffer(long cmd, int binding, long buffer, long offset);
    public static native int    nativeCmdSetViewport(long cmd, float x, float y, float w, float h);
    public static native int    nativeCmdSetScissor(long cmd, int x, int y, int w, int h);
    public static native int    nativeCmdDraw(long cmd, int vertexCount, int instanceCount, int firstVertex, int firstInstance);
    public static native int    nativeQueueSubmit(long queue, long[] commandBuffers);

    /* ---------------------------------------------------------------- */
    /* Public convenience API                                           */
    /* ---------------------------------------------------------------- */

    public static String getVersionString() { return nativeGetVersionString(); }
    public static String getBuildInfo()     { return nativeGetBuildInfo(); }
    public static String resultString(int r) { return nativeResultString(r); }
    public static int    detectCpuArch()    { return nativeMathDetectCpuArch(); }
    public static String archName(int a)    { return nativeMathArchName(a); }

    public static XPUInstance createInstance(int backend, boolean validation, boolean debug,
                                              String appName, String engineName) {
        long h = nativeCreateInstance(backend, validation, debug, appName, engineName);
        if (h == 0) throw new RuntimeException("XPU: failed to create instance");
        return new XPUInstance(h);
    }

    public static class PhysicalDeviceInfo {
        public int    vendor;
        public int    deviceType;
        public int    cpuArch;
        public String deviceName;
        public String driverName;
        public int    driverVersion;
        public long   totalMemoryBytes;
        public int    maxComputeUnits;
        public int    maxWorkgroupSize;
        public boolean supportsCompute;
        public boolean supportsGeometry;
        public boolean supportsTessellation;
        public boolean supportsMeshShaders;
    }
}
