// XPU - bindings/java/XPUQueue.java
package com.openxpu;

public final class XPUQueue {
    private final long handle;

    XPUQueue(long handle) { this.handle = handle; }

    public long getNativeHandle() { return handle; }

    public void submit(XPUCommandBuffer... cmds) {
        long[] handles = new long[cmds.length];
        for (int i = 0; i < cmds.length; ++i) handles[i] = cmds[i].getNativeHandle();
        XPU.nativeQueueSubmit(handle, handles);
    }

    public void waitIdle() {
        XPU.nativeQueueWaitIdle(handle);
    }
}
