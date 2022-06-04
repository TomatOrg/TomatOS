namespace System;

/// <summary>
/// Represents a 16-bit signed integer.
/// </summary>
public readonly struct Int16
{

    /// <summary>
    /// Represents the largest possible value of an Int16. This field is constant.
    /// </summary>
    public const short MaxValue = 32767;

    /// <summary>
    /// Represents the smallest possible value of Int16. This field is constant.
    /// </summary>
    public const short MinValue = -32768;
    
#pragma warning disable 169
    private readonly short _value;
#pragma warning restore 169
}