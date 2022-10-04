using System.Runtime.CompilerServices;

namespace System;

internal static class Buffer
{

    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    public static extern void Memmove(ref byte dest, ref byte src, nuint len);
    
    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    public static extern void _ZeroMemory(ref byte b, nuint byteLength);

}