using System.Runtime.CompilerServices;

namespace Tomato.DriverServices;

public static class KernelUtils
{

    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Native)]
    internal static extern bool GetNextFramebuffer(ref int index, out ulong addr, out int width, out int height, out int pitch);

    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Native)]
    internal static extern void GetKbdLayout(out ulong addr, out ulong size);
    
    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Native)]
    internal static extern void GetDefaultFont(out ulong addr, out int size);

}