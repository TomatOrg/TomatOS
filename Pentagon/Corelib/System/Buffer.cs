using System.Runtime.CompilerServices;

namespace System;

internal static class Buffer
{

    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    public static extern void Memmove(ref byte destination, ref byte source, nuint elementCount);

    public static void Memmove(ref char destination, ref char source, nuint elementCount)
    {
        Memmove(ref Unsafe.As<char, byte>(ref destination), ref Unsafe.As<char, byte>(ref source), elementCount * sizeof(char));
    }

    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    public static extern void _ZeroMemory(ref byte b, nuint byteLength);

}