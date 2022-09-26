// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

// ===================================================================================================
// Portions of the code implemented below are based on the 'Berkeley SoftFloat Release 3e' algorithms.
// ===================================================================================================

/*============================================================
**
**
**
** Purpose: Some floating-point math operations
**
**
===========================================================*/

using System.Runtime.CompilerServices;

namespace System;

/// <summary>
/// Provides constants and static methods for trigonometric, logarithmic, and other
/// common mathematical functions.
/// </summary>
public static class Math
{

    /// <summary>
    /// Represents the natural logarithmic base, specified by the constant, e.
    /// </summary>
    public const double E = 2.7182818284590451;
    
    /// <summary>
    /// Represents the ratio of the circumference of a circle to its diameter, specified by the
    /// constant, π.
    /// </summary>
    public const double PI = 3.1415926535897931;
    
    /// <summary>
    /// Represents the number of radians in one turn, specified by the constant, τ.
    /// </summary>
    public const double Tau = 6.2831853071795862;

    #region ABS
    
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static short Abs(short value)
    {
        if (value < 0)
        {
            value = (short)-value;
            if (value < 0)
            {
                ThrowAbsOverflow();
            }
        }
        return value;
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static int Abs(int value)
    {
        if (value < 0)
        {
            value = -value;
            if (value < 0)
            {
                ThrowAbsOverflow();
            }
        }
        return value;
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static long Abs(long value)
    {
        if (value < 0)
        {
            value = -value;
            if (value < 0)
            {
                ThrowAbsOverflow();
            }
        }
        return value;
    }

    /// <summary>Returns the absolute value of a native signed integer.</summary>
    /// <param name="value">A number that is greater than <see cref="IntPtr.MinValue" />, but less than or equal to <see cref="IntPtr.MaxValue" />.</param>
    /// <returns>A native signed integer, x, such that 0 ≤ x ≤ <see cref="IntPtr.MaxValue" />.</returns>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static nint Abs(nint value)
    {
        if (value < 0)
        {
            value = -value;
            if (value < 0)
            {
                ThrowAbsOverflow();
            }
        }
        return value;
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static sbyte Abs(sbyte value)
    {
        if (value < 0)
        {
            value = (sbyte)-value;
            if (value < 0)
            {
                ThrowAbsOverflow();
            }
        }
        return value;
    }

    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Native)]
    public static extern double Abs(double value);

    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Native)]
    public static extern float Abs(float value);

    #endregion
    
    #region Max

    public static nuint Max(nuint val1, nuint val2)
    {
        return val1 > val2 ? val1 : val2;
    }

    public static uint Max(uint val1, uint val2)
    {
        return val1 > val2 ? val1 : val2;
    }

    public static ushort Max(ushort val1, ushort val2)
    {
        return val1 > val2 ? val1 : val2;
    }

    public static float Max(float val1, float val2)
    {
        return val1 > val2 ? val1 : val2;
    }

    public static sbyte Max(sbyte val1, sbyte val2)
    {
        return val1 > val2 ? val1 : val2;
    }

    public static nint Max(nint val1, nint val2)
    {
        return val1 > val2 ? val1 : val2;
    }

    public static ulong Max(ulong val1, ulong val2)
    {
        return val1 > val2 ? val1 : val2;
    }

    public static int Max(int val1, int val2)
    {
        return val1 > val2 ? val1 : val2;
    }

    public static short Max(short val1, short val2)
    {
        return val1 > val2 ? val1 : val2;
    }

    public static double Max(double val1, double val2)
    {
        return val1 > val2 ? val1 : val2;
    }
    
    // TODO: decimal

    public static byte Max(byte val1, byte val2)
    {
        return val1 > val2 ? val1 : val2;
    }

    public static long Max(long val1, long val2)
    {
        return val1 > val2 ? val1 : val2;
    }

    #endregion

    #region Min

    public static byte Min(byte val1, byte val2)
    {
        return (val1 <= val2) ? val1 : val2;
    }
    
    public static short Min(short val1, short val2)
    {
        return (val1 <= val2) ? val1 : val2;
    }

    public static int Min(int val1, int val2)
    {
        return val1 < val2 ? val1 : val2;
    }

    public static long Min(long val1, long val2)
    {
        return (val1 <= val2) ? val1 : val2;
    }
    
    public static nint Min(nint val1, nint val2)
    {
        return (val1 <= val2) ? val1 : val2;
    }
    
    public static sbyte Min(sbyte val1, sbyte val2)
    {
        return (val1 <= val2) ? val1 : val2;
    }
    
    public static ushort Min(ushort val1, ushort val2)
    {
        return (val1 <= val2) ? val1 : val2;
    }
    
    public static uint Min(uint val1, uint val2)
    {
        return (val1 <= val2) ? val1 : val2;
    }
    
    public static ulong Min(ulong val1, ulong val2)
    {
        return (val1 <= val2) ? val1 : val2;
    }
    
    public static nuint Min(nuint val1, nuint val2)
    {
        return (val1 <= val2) ? val1 : val2;
    }
    
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static float Min(float val1, float val2)
    {
        // This matches the IEEE 754:2019 `minimum` function
        //
        // It propagates NaN inputs back to the caller and
        // otherwise returns the lesser of the inputs. It
        // treats -0 as lesser than +0 as per the specification.

        if (val1 != val2)
        {
            if (!float.IsNaN(val1))
            {
                return val1 < val2 ? val1 : val2;
            }

            return val1;
        }

        return float.IsNegative(val1) ? val1 : val2;
    }
    
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static double Min(double val1, double val2)
    {
        // This matches the IEEE 754:2019 `minimum` function
        //
        // It propagates NaN inputs back to the caller and
        // otherwise returns the lesser of the inputs. It
        // treats -0 as lesser than +0 as per the specification.

        if (val1 != val2)
        {
            if (!double.IsNaN(val1))
            {
                return val1 < val2 ? val1 : val2;
            }

            return val1;
        }

        return double.IsNegative(val1) ? val1 : val2;
    }

    #endregion

    private static void ThrowAbsOverflow()
    {
        throw new OverflowException(OverflowException.NegateTwosCompNum);
    }
}