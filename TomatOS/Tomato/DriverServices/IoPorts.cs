using System.Runtime.CompilerServices;

namespace Tomato.DriverServices;

internal static class IoPorts
{

    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    internal static extern byte In8(ushort port);

    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    internal static extern void Out8(ushort port, byte value);
}