// XPU - bindings/java/XPUDevice.java
package com.openxpu;

public final class XPUDevice implements AutoCloseable {
    private final XPUInstance instance;
    private long handle;

    XPUDevice(XPUInstance instance, long handle) {
        this.instance = instance;
        this.handle = handle;
    }

    public long getNativeHandle() { return handle; }

    public XPUQueue getQueue(int index) {
        long h = XPU.nativeGetDeviceQueue(handle, index);
        if (h == 0) throw new RuntimeException("XPU: failed to get queue");
        return new XPUQueue(h);
    }

    public XPUBuffer createBuffer(long size, int usage, int memory, boolean persistentlyMapped) {
        long h = XPU.nativeCreateBuffer(handle, size, usage, memory, persistentlyMapped);
        if (h == 0) throw new RuntimeException("XPU: failed to create buffer");
        return new XPUBuffer(h, size);
    }

    public XPUShader createVertexShader(String source) {
        return createShader(XPU.class, 1 /* vertex */, source);
    }

    public XPUShader createFragmentShader(String source) {
        return createShader(XPU.class, 8 /* fragment */, source);
    }

    public XPUShader createComputeShader(String source) {
        return createShader(XPU.class, 16 /* compute */, source);
    }

    private XPUShader createShader(Class<?> ignored, int stage, String source) {
        long h = XPU.nativeCreateShader(handle, stage, source, "main");
        if (h == 0) throw new RuntimeException("XPU: failed to create shader");
        return new XPUShader(h);
    }

    public XPUPipeline createGraphicsPipeline(XPUShader vertexShader,
                                                XPUShader fragmentShader,
                                                int topology,
                                                int colorFormat) {
        long h = XPU.nativeCreateGraphicsPipeline(handle,
                                                    vertexShader.getNativeHandle(),
                                                    fragmentShader.getNativeHandle(),
                                                    topology, colorFormat);
        if (h == 0) throw new RuntimeException("XPU: failed to create pipeline");
        return new XPUPipeline(h);
    }

    public XPUCommandBuffer createCommandBuffer() {
        long h = XPU.nativeCreateCommandBuffer(handle);
        if (h == 0) throw new RuntimeException("XPU: failed to create command buffer");
        return new XPUCommandBuffer(h);
    }

    public void waitIdle() {
        XPU.nativeDeviceWaitIdle(handle);
    }

    @Override
    public void close() {
        if (handle != 0) {
            XPU.nativeDestroyDevice(handle);
            handle = 0;
        }
    }

    @Override
    protected void finalize() throws Throwable {
        try { close(); } finally { super.finalize(); }
    }
}
