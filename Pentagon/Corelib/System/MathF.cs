// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

// ===================================================================================================
// Portions of the code implemented below are based on the 'Berkeley SoftFloat Release 3e' algorithms.
// ===================================================================================================

/*============================================================
**
** Purpose: Some single-precision floating-point math operations
**
===========================================================*/

using System.Runtime.CompilerServices;

namespace System;

public static class MathF
{
    
    public const float E = 2.71828183f;

    public const float PI = 3.14159265f;

    public const float Tau = 6.283185307f;

    // [MethodImpl(MethodImplOptions.AggressiveInlining)]
    // public static float Abs(float x)
    // {
    //     return Math.Abs(x);
    // }
    
    // [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Native)]
    // public static extern float Sqrt(float x);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static float Min(float x, float y)
    {
        return Math.Min(x, y);
    }
    
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static float Max(float x, float y)
    {
        return Math.Max(x, y);
    }

}