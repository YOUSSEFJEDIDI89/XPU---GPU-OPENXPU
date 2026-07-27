// XPU - bindings/java/XPUInstance.java
//
// Java wrapper for an XPU instance handle.

package com.openxpu;

import java.util.ArrayList;
import java.util.List;

public final class XPUInstance implements AutoCloseable {
    private long handle;

    XPUInstance(long handle) { this.handle = handle; }

    public long getNativeHandle() { return handle; }

    public int getActiveBackend() {
        return XPU.nativeGetActiveBackend(handle);
    }

    public XPUPhysicalDevice[] enumeratePhysicalDevices() {
        /* First call to get count */
        int count = XPU.nativeEnumeratePhysicalDevices(handle, null);
        if (count <= 0) return new XPUPhysicalDevice[0];
        long[] handles = new long[count];
        int actual = XPU.nativeEnumeratePhysicalDevices(handle, handles);
        XPUPhysicalDevice[] result = new XPUPhysicalDevice[actual];
        for (int i = 0; i < actual; ++i) {
            result[i] = new XPUPhysicalDevice(this, handles[i]);
        }
        return result;
    }

    @Override
    public void close() {
        if (handle != 0) {
            XPU.nativeDestroyInstance(handle);
            handle = 0;
        }
    }

    @Override
    protected void finalize() throws Throwable {
        try { close(); } finally { super.finalize(); }
    }
}
