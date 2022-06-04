namespace System;

/// <summary>
/// Represents a 32-bit signed integer.
/// </summary>
public readonly struct Int32
{
    
    /// <summary>
    /// Represents the largest possible value of an Int32. This field is constant.
    /// </summary>
    public const int MaxValue = 2147483647;
    
    /// <summary>
    /// Represents the smallest possible value of Int32. This field is constant.
    /// </summary>
    public const int MinValue = -2147483648;
    
#pragma warning disable 169
    private readonly int _value;
#pragma warning restore 169
}