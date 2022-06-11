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
    
}