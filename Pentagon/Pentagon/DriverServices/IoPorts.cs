using System.Runtime.CompilerServices;

namespace Pentagon.DriverServices;

public class IoPorts
{

    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    internal static extern byte In8(ushort port);
}