// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System.Diagnostics.CodeAnalysis;
using System.Globalization;

namespace System;

public readonly struct Byte : ISpanFormattable, IComparable<byte>, IEquatable<byte>
{

    private readonly byte m_value;

    // The maximum value that a Byte may represent: 255.
    public const byte MaxValue = (byte)0xFF;

    // The minimum value that a Byte may represent: 0.
    public const byte MinValue = 0;
    
    public int CompareTo(byte value)
    {
        return m_value - value;
    }

    // Determines whether two Byte objects are equal.
    public override bool Equals([NotNullWhen(true)] object? obj)
    {
        if (obj is not byte b)
        {
            return false;
        }
        return m_value == b.m_value;
    }

    public bool Equals(byte obj)
    {
        return m_value == obj;
    }

    // Gets a hash code for this instance.
    public override int GetHashCode()
    {
        return m_value;
    }

    public static byte Parse(string s)
    {
        if (s == null) ThrowHelper.ThrowArgumentNullException(ExceptionArgument.s);
        return Parse((ReadOnlySpan<char>)s, NumberStyles.Integer, NumberFormatInfo.CurrentInfo);
    }

    public static byte Parse(string s, NumberStyles style)
    {
        NumberFormatInfo.ValidateParseStyleInteger(style);
        if (s == null) ThrowHelper.ThrowArgumentNullException(ExceptionArgument.s);
        return Parse((ReadOnlySpan<char>)s, style, NumberFormatInfo.CurrentInfo);
    }

    public static byte Parse(string s, IFormatProvider? provider)
    {
        if (s == null) ThrowHelper.ThrowArgumentNullException(ExceptionArgument.s);
        return Parse((ReadOnlySpan<char>)s, NumberStyles.Integer, NumberFormatInfo.GetInstance(provider));
    }

    // Parses an unsigned byte from a String in the given style.  If
    // a NumberFormatInfo isn't specified, the current culture's
    // NumberFormatInfo is assumed.
    public static byte Parse(string s, NumberStyles style, IFormatProvider? provider)
    {
        NumberFormatInfo.ValidateParseStyleInteger(style);
        if (s == null) ThrowHelper.ThrowArgumentNullException(ExceptionArgument.s);
        return Parse((ReadOnlySpan<char>)s, style, NumberFormatInfo.GetInstance(provider));
    }

    public static byte Parse(ReadOnlySpan<char> s, NumberStyles style = NumberStyles.Integer, IFormatProvider? provider = null)
    {
        NumberFormatInfo.ValidateParseStyleInteger(style);
        return Parse(s, style, NumberFormatInfo.GetInstance(provider));
    }

    private static byte Parse(ReadOnlySpan<char> s, NumberStyles style, NumberFormatInfo info)
    {
        Number.ParsingStatus status = Number.TryParseUInt32(s, style, info, out uint i);
        if (status != Number.ParsingStatus.OK)
        {
            Number.ThrowOverflowOrFormatException(status, TypeCode.Byte);
        }

        if (i > MaxValue) Number.ThrowOverflowException(TypeCode.Byte);
        return (byte)i;
    }

    public static bool TryParse([NotNullWhen(true)] string? s, out byte result)
    {
        if (s == null)
        {
            result = 0;
            return false;
        }

        return TryParse((ReadOnlySpan<char>)s, NumberStyles.Integer, NumberFormatInfo.CurrentInfo, out result);
    }

    public static bool TryParse(ReadOnlySpan<char> s, out byte result)
    {
        return TryParse(s, NumberStyles.Integer, NumberFormatInfo.CurrentInfo, out result);
    }

    public static bool TryParse([NotNullWhen(true)] string? s, NumberStyles style, IFormatProvider? provider, out byte result)
    {
        NumberFormatInfo.ValidateParseStyleInteger(style);

        if (s == null)
        {
            result = 0;
            return false;
        }

        return TryParse((ReadOnlySpan<char>)s, style, NumberFormatInfo.GetInstance(provider), out result);
    }

    public static bool TryParse(ReadOnlySpan<char> s, NumberStyles style, IFormatProvider? provider, out byte result)
    {
        NumberFormatInfo.ValidateParseStyleInteger(style);
        return TryParse(s, style, NumberFormatInfo.GetInstance(provider), out result);
    }

    private static bool TryParse(ReadOnlySpan<char> s, NumberStyles style, NumberFormatInfo info, out byte result)
    {
        if (Number.TryParseUInt32(s, style, info, out uint i) != Number.ParsingStatus.OK
            || i > MaxValue)
        {
            result = 0;
            return false;
        }
        result = (byte)i;
        return true;
    }

    public override string ToString()
    {
        return Number.UInt32ToDecStr(m_value);
    }

    public string ToString(string? format)
    {
        return Number.FormatUInt32(m_value, format, null);
    }

    public string ToString(IFormatProvider? provider)
    {
        return Number.UInt32ToDecStr(m_value);
    }

    public string ToString(string? format, IFormatProvider? provider)
    {
        return Number.FormatUInt32(m_value, format, provider);
    }

    public bool TryFormat(Span<char> destination, out int charsWritten, ReadOnlySpan<char> format = default, IFormatProvider? provider = null)
    {
        return Number.TryFormatUInt32(m_value, format, provider, destination, out charsWritten);
    }


    
}