// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System.Runtime.CompilerServices;

namespace System;

/// <summary>
/// Converts base data types to an array of bytes, and an array of bytes to base data types.
/// </summary>
public static class BitConverter
{
    
    // This field indicates the "endianess" of the architecture.
    // The value is set to true if the architecture is
    // little endian; false if it is big endian.
    public static readonly bool IsLittleEndian = true;

    /// <summary>
    /// Converts the specified single-precision floating point number to a 32-bit signed integer.
    /// </summary>
    /// <param name="value">The number to convert.</param>
    /// <returns>A 32-bit signed integer whose bits are identical to <paramref name="value"/>.</returns>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static int SingleToInt32Bits(float value)
    {
        // TODO: support for ref on value arguments 
        var val = value;
        return Unsafe.As<float, int>(ref val);
    }

    /// <summary>
    /// Converts the specified double-precision floating point number to a 64-bit signed integer.
    /// </summary>
    /// <param name="value">The number to convert.</param>
    /// <returns>A 64-bit signed integer whose bits are identical to <paramref name="value"/>.</returns>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static long DoubleToInt64Bits(double value)
    {
        // TODO: support for ref on value arguments 
        var val = value;
        return Unsafe.As<double, long>(ref val);
    }

}