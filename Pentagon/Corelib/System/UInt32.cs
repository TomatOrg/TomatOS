// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System.Diagnostics.CodeAnalysis;
using System.Globalization;

namespace System;

public readonly struct UInt32 : ISpanFormattable, IComparable<uint>, IEquatable<uint>
{
    private readonly uint m_value;

    public const uint MaxValue = (uint)0xffffffff;
    public const uint MinValue = 0U;

    public int CompareTo(uint value)
    {
        // Need to use compare because subtraction will wrap
        // to positive for very large neg numbers, etc.
        if (m_value < value) return -1;
        return m_value > value ? 1 : 0;
    }

    public override bool Equals([NotNullWhen(true)] object? obj)
    {
        if (obj is not uint u)
        {
            return false;
        }
        return m_value == u.m_value;
    }

    public bool Equals(uint obj)
    {
        return m_value == obj;
    }

    // The absolute value of the int contained.
    public override int GetHashCode()
    {
        return (int)m_value;
    }

    // The base 10 representation of the number with no extra padding.
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
    
    public static uint Parse(string s)
    {
        if (s == null) ThrowHelper.ThrowArgumentNullException(ExceptionArgument.s);
        return Number.ParseUInt32(s, NumberStyles.Integer, NumberFormatInfo.CurrentInfo);
    }

    public static uint Parse(string s, NumberStyles style)
    {
        NumberFormatInfo.ValidateParseStyleInteger(style);
        if (s == null) ThrowHelper.ThrowArgumentNullException(ExceptionArgument.s);
        return Number.ParseUInt32(s, style, NumberFormatInfo.CurrentInfo);
    }

    public static uint Parse(string s, IFormatProvider? provider)
    {
        if (s == null) ThrowHelper.ThrowArgumentNullException(ExceptionArgument.s);
        return Number.ParseUInt32(s, NumberStyles.Integer, NumberFormatInfo.GetInstance(provider));
    }

    public static uint Parse(string s, NumberStyles style, IFormatProvider? provider)
    {
        NumberFormatInfo.ValidateParseStyleInteger(style);
        if (s == null) ThrowHelper.ThrowArgumentNullException(ExceptionArgument.s);
        return Number.ParseUInt32(s, style, NumberFormatInfo.GetInstance(provider));
    }

    public static uint Parse(ReadOnlySpan<char> s, NumberStyles style = NumberStyles.Integer, IFormatProvider? provider = null)
    {
        NumberFormatInfo.ValidateParseStyleInteger(style);
        return Number.ParseUInt32(s, style, NumberFormatInfo.GetInstance(provider));
    }

    public static bool TryParse([NotNullWhen(true)] string? s, out uint result)
    {
        if (s == null)
        {
            result = 0;
            return false;
        }

        return Number.TryParseUInt32IntegerStyle(s, NumberStyles.Integer, NumberFormatInfo.CurrentInfo, out result) == Number.ParsingStatus.OK;
    }

    public static bool TryParse(ReadOnlySpan<char> s, out uint result)
    {
        return Number.TryParseUInt32IntegerStyle(s, NumberStyles.Integer, NumberFormatInfo.CurrentInfo, out result) == Number.ParsingStatus.OK;
    }

    public static bool TryParse([NotNullWhen(true)] string? s, NumberStyles style, IFormatProvider? provider, out uint result)
    {
        NumberFormatInfo.ValidateParseStyleInteger(style);

        if (s == null)
        {
            result = 0;
            return false;
        }

        return Number.TryParseUInt32(s, style, NumberFormatInfo.GetInstance(provider), out result) == Number.ParsingStatus.OK;
    }

    public static bool TryParse(ReadOnlySpan<char> s, NumberStyles style, IFormatProvider? provider, out uint result)
    {
        NumberFormatInfo.ValidateParseStyleInteger(style);
        return Number.TryParseUInt32(s, style, NumberFormatInfo.GetInstance(provider), out result) == Number.ParsingStatus.OK;
    }


}