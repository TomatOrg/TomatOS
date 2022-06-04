namespace System;

public readonly struct Byte
{

    /// <summary>
    /// Represents the largest possible value of a Byte. This field is constant.
    /// </summary>
    public const byte MaxValue = 255;

    /// <summary>
    /// Represents the smallest possible value of a Byte. This field is constant.
    /// </summary>
    public const byte MinValue = 0;
    
#pragma warning disable 169
    private readonly byte _value;
#pragma warning restore 169
}