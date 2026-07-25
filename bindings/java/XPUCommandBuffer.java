// XPU - bindings/java/XPUCommandBuffer.java
package com.openxpu;

public final class XPUCommandBuffer implements AutoCloseable {
    private long handle;

    XPUCommandBuffer(long handle) { this.handle = handle; }

    public long getNativeHandle() { return handle; }

    public void begin() { XPU.nativeBeginCommandBuffer(handle); }
    public void end()   { XPU.nativeEndCommandBuffer(handle); }

    public void bindPipeline(XPUPipeline pipeline) {
        XPU.nativeCmdBindPipeline(handle, pipeline.getNativeHandle());
    }

    public void bindVertexBuffer(int binding, XPUBuffer buffer, long offset) {
        XPU.nativeCmdBindVertexBuffer(handle, binding, buffer.getNativeHandle(), offset);
    }

    public void setViewport(float x, float y, float w, float h) {
        XPU.nativeCmdSetViewport(handle, x, y, w, h);
    }

    public void setScissor(int x, int y, int w, int h) {
        XPU.nativeCmdSetScissor(handle, x, y, w, h);
    }

    public void draw(int vertexCount, int instanceCount, int firstVertex, int firstInstance) {
        XPU.nativeCmdDraw(handle, vertexCount, instanceCount, firstVertex, firstInstance);
    }

    @Override
    public void close() {
        if (handle != 0) {
            XPU.nativeDestroyCommandBuffer(handle);
            handle = 0;
        }
    }

    @Override
    protected void finalize() throws Throwable {
        try { close(); } finally { super.finalize(); }
    }
}
