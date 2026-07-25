// XPU - bindings/java/XPUPhysicalDevice.java
package com.openxpu;

public final class XPUPhysicalDevice {
    private final XPUInstance instance;
    private final long handle;

    XPUPhysicalDevice(XPUInstance instance, long handle) {
        this.instance = instance;
        this.handle = handle;
    }

    public long getNativeHandle() { return handle; }

    public XPU.PhysicalDeviceInfo getInfo() {
        return XPU.nativeGetPhysicalDeviceInfo(handle);
    }

    public XPUDevice createDevice(boolean enableCompute) {
        long h = XPU.nativeCreateDevice(handle, enableCompute);
        if (h == 0) throw new RuntimeException("XPU: failed to create device");
        return new XPUDevice(instance, h);
    }
}
