// XPU - bindings/csharp/XPU.cs
//
// C# wrapper around the XPU C API. Uses P/Invoke (DllImport) so it works
// on .NET Framework, .NET Core, .NET 5+, Mono, and Unity.
//
// Usage:
//   using OpenXpu;
//   var instance = Xpu.CreateInstance(backend: XpuBackend.Auto);
//   var devices  = instance.EnumeratePhysicalDevices();
//   var device   = devices[0].CreateDevice(enableCompute: true);
//   ...

using System;
using System.Runtime.InteropServices;
using System.Text;

namespace OpenXpu
{
    /// <summary>
    /// XPU static API - mirrors the C entry points. Higher-level OO wrappers
    /// (XpuInstance, XpuDevice, ...) wrap these calls.
    /// </summary>
    public static class Xpu
    {
        private const string LibName = "xpu";

        // ----------------------------------------------------------------
        // Constants
        // ----------------------------------------------------------------
        public const int SUCCESS                   = 0;
        public const int ERROR_NOT_READY           = 1;
        public const int ERROR_OUT_OF_MEMORY       = 2;
        public const int ERROR_INVALID_ARG         = 3;
        public const int ERROR_DEVICE_LOST         = 4;
        public const int ERROR_UNSUPPORTED         = 5;
        public const int ERROR_FORMAT_MISMATCH     = 6;
        public const int ERROR_SHADER_COMPILE      = 7;
        public const int ERROR_BACKEND             = 8;
        public const int ERROR_PLATFORM            = 9;

        // ----------------------------------------------------------------
        // Version
        // ----------------------------------------------------------------
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern uint xpuGetVersion();

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr xpuGetVersionString();

        public static string GetVersionString()
            => Marshal.PtrToStringAnsi(xpuGetVersionString()) ?? "";

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr xpuGetBuildInfo();

        public static string GetBuildInfo()
            => Marshal.PtrToStringAnsi(xpuGetBuildInfo()) ?? "";

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr xpuResultString(int result);

        public static string ResultString(int r)
            => Marshal.PtrToStringAnsi(xpuResultString(r)) ?? "";

        // ----------------------------------------------------------------
        // CPU detection
        // ----------------------------------------------------------------
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int xpu_math_detect_cpu_arch();

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr xpu_math_arch_name(int arch);

        public static string ArchName(int arch)
            => Marshal.PtrToStringAnsi(xpu_math_arch_name(arch)) ?? "";
    }

    public enum XpuBackend : int
    {
        Auto        = 0,
        Software    = 1,
        OpenGlEs    = 2,
        OpenGlCore  = 3,
        Vulkan      = 4,
        Metal       = 5,
        DirectX12   = 6
    }

    public enum XpuDeviceType : int
    {
        Unknown    = 0,
        Cpu        = 1,
        Integrated = 2,
        Discrete   = 3,
        Virtual    = 4,
        XpuNative  = 5
    }

    public enum XpuCpuArch : int
    {
        Unknown    = 0,
        X86Sse2    = 1,
        X86Avx2    = 2,
        X86Avx512  = 3,
        ArmNeon    = 4,
        ArmNeon64  = 5,
        ArmVfp     = 6
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct XpuPhysicalDeviceInfo
    {
        public int Vendor;
        public int DeviceType;
        public int CpuArch;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 256)]
        public string DeviceName;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 64)]
        public string DriverName;
        public uint DriverVersion;
        public ulong TotalMemoryBytes;
        public uint MaxComputeUnits;
        public uint MaxWorkgroupSize;
        public int SupportsCompute;
        public int SupportsGeometry;
        public int SupportsTessellation;
        public int SupportsMeshShaders;
    }

    /// <summary>
    /// OOO wrapper for an XPU instance.
    /// </summary>
    public sealed class XpuInstance : IDisposable
    {
        [DllImport("xpu", CallingConvention = CallingConvention.Cdecl)]
        private static extern int xpuCreateInstance(ref XpuInstanceCreateInfo ci, out IntPtr inst);

        [DllImport("xpu", CallingConvention = CallingConvention.Cdecl)]
        private static extern void xpuDestroyInstance(IntPtr inst);

        [DllImport("xpu", CallingConvention = CallingConvention.Cdecl)]
        private static extern int xpuGetActiveBackend(IntPtr inst);

        [DllImport("xpu", CallingConvention = CallingConvention.Cdecl)]
        private static extern int xpuEnumeratePhysicalDevices(IntPtr inst, ref uint count, IntPtr[] devices);

        [DllImport("xpu", CallingConvention = CallingConvention.Cdecl)]
        private static extern int xpuGetPhysicalDeviceInfo(IntPtr pd, out XpuPhysicalDeviceInfo info);

        [StructLayout(LayoutKind.Sequential)]
        private struct XpuInstanceCreateInfo
        {
            public int PreferredBackend;
            public int EnableValidation;
            public int EnableDebug;
            [MarshalAs(UnmanagedType.LPStr)] public string AppName;
            public uint AppVersion;
            [MarshalAs(UnmanagedType.LPStr)] public string EngineName;
            public uint EngineVersion;
        }

        public IntPtr Handle { get; private set; }

        private XpuInstance(IntPtr h) { Handle = h; }

        public static XpuInstance Create(XpuBackend backend = XpuBackend.Auto,
                                          bool validation = false,
                                          bool debug = false,
                                          string appName = "openxpu",
                                          string engineName = "openxpu")
        {
            var ci = new XpuInstanceCreateInfo
            {
                PreferredBackend  = (int)backend,
                EnableValidation  = validation ? 1 : 0,
                EnableDebug       = debug ? 1 : 0,
                AppName           = appName,
                AppVersion        = 1,
                EngineName        = engineName,
                EngineVersion     = 1
            };
            IntPtr h;
            int r = xpuCreateInstance(ref ci, out h);
            if (r != Xpu.SUCCESS)
                throw new InvalidOperationException($"xpuCreateInstance failed: {Xpu.ResultString(r)}");
            return new XpuInstance(h);
        }

        public XpuBackend ActiveBackend => (XpuBackend)xpuGetActiveBackend(Handle);

        public IntPtr[] EnumeratePhysicalDevices()
        {
            uint count = 0;
            xpuEnumeratePhysicalDevices(Handle, ref count, null);
            if (count == 0) return Array.Empty<IntPtr>();
            var arr = new IntPtr[count];
            xpuEnumeratePhysicalDevices(Handle, ref count, arr);
            return arr;
        }

        public XpuPhysicalDeviceInfo GetPhysicalDeviceInfo(IntPtr pd)
        {
            XpuPhysicalDeviceInfo info;
            int r = xpuGetPhysicalDeviceInfo(pd, out info);
            if (r != Xpu.SUCCESS)
                throw new InvalidOperationException($"xpuGetPhysicalDeviceInfo failed: {Xpu.ResultString(r)}");
            return info;
        }

        public void Dispose()
        {
            if (Handle != IntPtr.Zero)
            {
                xpuDestroyInstance(Handle);
                Handle = IntPtr.Zero;
            }
            GC.SuppressFinalize(this);
        }

        ~XpuInstance() { Dispose(); }
    }

    /// <summary>
    /// OO wrapper for an XPU logical device.
    /// </summary>
    public sealed class XpuDevice : IDisposable
    {
        [DllImport("xpu", CallingConvention = CallingConvention.Cdecl)]
        private static extern int xpuCreateDevice(IntPtr pd, ref XpuDeviceCreateInfo ci, out IntPtr dev);

        [DllImport("xpu", CallingConvention = CallingConvention.Cdecl)]
        private static extern void xpuDestroyDevice(IntPtr dev);

        [DllImport("xpu", CallingConvention = CallingConvention.Cdecl)]
        private static extern int xpuGetDeviceQueue(IntPtr dev, uint index, out IntPtr queue);

        [DllImport("xpu", CallingConvention = CallingConvention.Cdecl)]
        private static extern int xpuDeviceWaitIdle(IntPtr dev);

        [StructLayout(LayoutKind.Sequential)]
        private struct XpuDeviceCreateInfo
        {
            public IntPtr PhysicalDevice;
            public uint QueueCount;
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 4)] public float[] QueuePriorities;
            public int EnableCompute;
        }

        public IntPtr Handle { get; private set; }

        private XpuDevice(IntPtr h) { Handle = h; }

        public static XpuDevice Create(IntPtr physicalDevice, bool enableCompute = true)
        {
            var ci = new XpuDeviceCreateInfo
            {
                PhysicalDevice  = physicalDevice,
                QueueCount      = 1,
                QueuePriorities = new[] { 1.0f, 0, 0, 0 },
                EnableCompute   = enableCompute ? 1 : 0
            };
            IntPtr h;
            int r = xpuCreateDevice(physicalDevice, ref ci, out h);
            if (r != Xpu.SUCCESS)
                throw new InvalidOperationException($"xpuCreateDevice failed: {Xpu.ResultString(r)}");
            return new XpuDevice(h);
        }

        public IntPtr GetQueue(uint index = 0)
        {
            IntPtr q;
            int r = xpuGetDeviceQueue(Handle, index, out q);
            if (r != Xpu.SUCCESS)
                throw new InvalidOperationException($"xpuGetDeviceQueue failed: {Xpu.ResultString(r)}");
            return q;
        }

        public void WaitIdle() => xpuDeviceWaitIdle(Handle);

        public void Dispose()
        {
            if (Handle != IntPtr.Zero)
            {
                xpuDestroyDevice(Handle);
                Handle = IntPtr.Zero;
            }
            GC.SuppressFinalize(this);
        }

        ~XpuDevice() { Dispose(); }
    }
}
