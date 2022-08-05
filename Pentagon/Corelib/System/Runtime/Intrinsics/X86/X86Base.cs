using System.Runtime.CompilerServices;

namespace System.Runtime.Intrinsics.X86;

public static class X86Base
{

    public static bool IsSupported => true;
    
    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    public static extern void Pause();

}