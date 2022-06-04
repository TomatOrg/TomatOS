namespace System;

/// <summary>
/// Represents a 64-bit signed integer.
/// </summary>
public readonly struct Int64
{
    
    /// <summary>
    /// Represents the largest possible value of an Int64. This field is constant.
    /// </summary>
    public const long MaxValue = 9223372036854775807;
    
    /// <summary>
    /// Represents the smallest possible value of an Int64. This field is constant.
    /// </summary>
    public const long MinValue = -9223372036854775808;
    
#pragma warning disable 169
    private readonly long _value;
#pragma warning restore 169
}