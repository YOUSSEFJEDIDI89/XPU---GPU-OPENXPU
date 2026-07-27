// XPU - bindings/java/XPUPipeline.java
package com.openxpu;

public final class XPUPipeline implements AutoCloseable {
    private long handle;

    XPUPipeline(long handle) { this.handle = handle; }

    public long getNativeHandle() { return handle; }

    @Override
    public void close() {
        if (handle != 0) {
            XPU.nativeDestroyPipeline(handle);
            handle = 0;
        }
    }

    @Override
    protected void finalize() throws Throwable {
        try { close(); } finally { super.finalize(); }
    }
}
