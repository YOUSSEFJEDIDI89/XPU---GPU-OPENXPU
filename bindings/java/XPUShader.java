// XPU - bindings/java/XPUShader.java
package com.openxpu;

public final class XPUShader implements AutoCloseable {
    private long handle;

    XPUShader(long handle) { this.handle = handle; }

    public long getNativeHandle() { return handle; }

    @Override
    public void close() {
        if (handle != 0) {
            XPU.nativeDestroyShader(handle);
            handle = 0;
        }
    }

    @Override
    protected void finalize() throws Throwable {
        try { close(); } finally { super.finalize(); }
    }
}
