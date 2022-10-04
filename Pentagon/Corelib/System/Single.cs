using System.Runtime.CompilerServices;

namespace System;

public readonly struct Single
{
#pragma warning disable 169
    private readonly float _value;
#pragma warning restore 169
    
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static unsafe bool IsNaN(float f)
    {
        // A NaN will never equal itself so this is an
        // easy and efficient way to check for NaN.
        return f != f;
    }
    
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static unsafe bool IsNegative(float f)
    {
        return BitConverter.SingleToInt32Bits(f) < 0;
    }
    
}