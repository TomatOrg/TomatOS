// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System.Diagnostics.CodeAnalysis;
using System.Globalization;

namespace System;

public readonly struct UInt16 : ISpanFormattable, IComparable<ushort>, IEquatable<ushort>
{
    private readonly ushort m_value;
    
    public const ushort MaxValue = (ushort)0xFFFF;
    public const ushort MinValue = 0;

    public int CompareTo(ushort value)
    {
        return (int)m_value - (int)value;
    }

    public override bool Equals([NotNullWhen(true)] object? obj)
    {
        if (obj is not ushort @ushort)
        {
            return false;
        }
        return m_value == @ushort.m_value;
    }

    public bool Equals(ushort obj)
    {
        return m_value == obj;
    }

    // Returns a HashCode for the UInt16
    public override int GetHashCode()
    {
        return (int)m_value;
    }

    // Converts the current value to a String in base-10 with no extra padding.
    public override string ToString()
    {
        return Number.UInt32ToDecStr(m_value);
    }

    public string ToString(IFormatProvider? provider)
    {
        return Number.UInt32ToDecStr(m_value);
    }

    public string ToString(string? format)
    {
        return Number.FormatUInt32(m_value, format, null);
    }

    public string ToString(string? format, IFormatProvider? provider)
    {
        return Number.FormatUInt32(m_value, format, provider);
    }

    public bool TryFormat(Span<char> destination, out int charsWritten, ReadOnlySpan<char> format = default, IFormatProvider? provider = null)
    {
        return Number.TryFormatUInt32(m_value, format, provider, destination, out charsWritten);
    }

    public static ushort Parse(string s)
    {
        if (s == null) ThrowHelper.ThrowArgumentNullException(ExceptionArgument.s);
        return Parse((ReadOnlySpan<char>)s, NumberStyles.Integer, NumberFormatInfo.CurrentInfo);
    }

    public static ushort Parse(string s, NumberStyles style)
    {
        NumberFormatInfo.ValidateParseStyleInteger(style);
        if (s == null) ThrowHelper.ThrowArgumentNullException(ExceptionArgument.s);
        return Parse((ReadOnlySpan<char>)s, style, NumberFormatInfo.CurrentInfo);
    }

    public static ushort Parse(string s, IFormatProvider? provider)
    {
        if (s == null) ThrowHelper.ThrowArgumentNullException(ExceptionArgument.s);
        return Parse((ReadOnlySpan<char>)s, NumberStyles.Integer, NumberFormatInfo.GetInstance(provider));
    }

    public static ushort Parse(string s, NumberStyles style, IFormatProvider? provider)
    {
        NumberFormatInfo.ValidateParseStyleInteger(style);
        if (s == null) ThrowHelper.ThrowArgumentNullException(ExceptionArgument.s);
        return Parse((ReadOnlySpan<char>)s, style, NumberFormatInfo.GetInstance(provider));
    }

    public static ushort Parse(ReadOnlySpan<char> s, NumberStyles style = NumberStyles.Integer, IFormatProvider? provider = null)
    {
        NumberFormatInfo.ValidateParseStyleInteger(style);
        return Parse(s, style, NumberFormatInfo.GetInstance(provider));
    }

    private static ushort Parse(ReadOnlySpan<char> s, NumberStyles style, NumberFormatInfo info)
    {
        Number.ParsingStatus status = Number.TryParseUInt32(s, style, info, out uint i);
        if (status != Number.ParsingStatus.OK)
        {
            Number.ThrowOverflowOrFormatException(status, TypeCode.UInt16);
        }

        if (i > MaxValue) Number.ThrowOverflowException(TypeCode.UInt16);
        return (ushort)i;
    }

    public static bool TryParse([NotNullWhen(true)] string? s, out ushort result)
    {
        if (s == null)
        {
            result = 0;
            return false;
        }

        return TryParse((ReadOnlySpan<char>)s, NumberStyles.Integer, NumberFormatInfo.CurrentInfo, out result);
    }

    public static bool TryParse(ReadOnlySpan<char> s, out ushort result)
    {
        return TryParse(s, NumberStyles.Integer, NumberFormatInfo.CurrentInfo, out result);
    }

    public static bool TryParse([NotNullWhen(true)] string? s, NumberStyles style, IFormatProvider? provider, out ushort result)
    {
        NumberFormatInfo.ValidateParseStyleInteger(style);

        if (s == null)
        {
            result = 0;
            return false;
        }

        return TryParse((ReadOnlySpan<char>)s, style, NumberFormatInfo.GetInstance(provider), out result);
    }

    public static bool TryParse(ReadOnlySpan<char> s, NumberStyles style, IFormatProvider? provider, out ushort result)
    {
        NumberFormatInfo.ValidateParseStyleInteger(style);
        return TryParse(s, style, NumberFormatInfo.GetInstance(provider), out result);
    }

    private static bool TryParse(ReadOnlySpan<char> s, NumberStyles style, NumberFormatInfo info, out ushort result)
    {
        if (Number.TryParseUInt32(s, style, info, out uint i) != Number.ParsingStatus.OK
            || i > MaxValue)
        {
            result = 0;
            return false;
        }
        result = (ushort)i;
        return true;
    }


}