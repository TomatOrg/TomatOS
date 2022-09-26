using System.Runtime.CompilerServices;

namespace System;

public readonly struct Double
{
#pragma warning disable 169
    private readonly double _value;
#pragma warning restore 169
    
    
    /// <summary>Determines whether the specified value is NaN.</summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static unsafe bool IsNaN(double d)
    {
        // A NaN will never equal itself so this is an
        // easy and efficient way to check for NaN.
        return d != d;
    }
    
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static unsafe bool IsNegative(double d)
    {
        return BitConverter.DoubleToInt64Bits(d) < 0;
    }
    
}