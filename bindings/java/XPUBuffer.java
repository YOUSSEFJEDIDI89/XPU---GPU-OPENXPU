// XPU - bindings/java/XPUBuffer.java
package com.openxpu;

public final class XPUBuffer implements AutoCloseable {
    private long handle;
    private final long size;

    public static final int USAGE_VERTEX       = 0x01;
    public static final int USAGE_INDEX        = 0x02;
    public static final int USAGE_UNIFORM      = 0x04;
    public static final int USAGE_STORAGE      = 0x08;
    public static final int USAGE_INDIRECT     = 0x10;
    public static final int USAGE_TRANSFER_SRC = 0x20;
    public static final int USAGE_TRANSFER_DST = 0x40;

    public static final int MEMORY_DEFAULT     = 0x00;
    public static final int MEMORY_GPU_ONLY    = 0x01;
    public static final int MEMORY_CPU_TO_GPU  = 0x02;
    public static final int MEMORY_GPU_TO_CPU  = 0x04;
    public static final int MEMORY_CPU_ONLY    = 0x08;
    public static final int MEMORY_PERSISTENT  = 0x10;

    XPUBuffer(long handle, long size) {
        this.handle = handle;
        this.size = size;
    }

    public long getNativeHandle() { return handle; }
    public long getSize() { return size; }

    public void update(byte[] data) {
        update(0, data);
    }

    public void update(long offset, byte[] data) {
        XPU.nativeUpdateBuffer(handle, offset, data);
    }

    @Override
    public void close() {
        if (handle != 0) {
            XPU.nativeDestroyBuffer(handle);
            handle = 0;
        }
    }

    @Override
    protected void finalize() throws Throwable {
        try { close(); } finally { super.finalize(); }
    }
}
