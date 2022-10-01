// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System.Diagnostics.CodeAnalysis;
using System.Globalization;

namespace System;

public readonly struct UInt64 : ISpanFormattable, IComparable<ulong>, IEquatable<ulong>
{
    private readonly ulong m_value;

    public const ulong MaxValue = (ulong)0xffffffffffffffffL;
    public const ulong MinValue = 0x0;
    
    public int CompareTo(ulong value)
    {
        // Need to use compare because subtraction will wrap
        // to positive for very large neg numbers, etc.
        if (m_value < value) return -1;
        if (m_value > value) return 1;
        return 0;
    }

    public override bool Equals([NotNullWhen(true)] object? obj)
    {
        if (obj is not ulong @ulong)
        {
            return false;
        }
        return m_value == @ulong.m_value;
    }

    public bool Equals(ulong obj)
    {
        return m_value == obj;
    }

    // The value of the lower 32 bits XORed with the uppper 32 bits.
    public override int GetHashCode()
    {
        return ((int)m_value) ^ (int)(m_value >> 32);
    }

    public override string ToString()
    {
        return Number.UInt64ToDecStr(m_value, -1);
    }

    public string ToString(IFormatProvider? provider)
    {
        return Number.UInt64ToDecStr(m_value, -1);
    }

    public string ToString(string? format)
    {
        return Number.FormatUInt64(m_value, format, null);
    }

    public string ToString(string? format, IFormatProvider? provider)
    {
        return Number.FormatUInt64(m_value, format, provider);
    }

    public bool TryFormat(Span<char> destination, out int charsWritten, ReadOnlySpan<char> format = default, IFormatProvider? provider = null)
    {
        return Number.TryFormatUInt64(m_value, format, provider, destination, out charsWritten);
    }

    public static ulong Parse(string s)
    {
        if (s == null) ThrowHelper.ThrowArgumentNullException(ExceptionArgument.s);
        return Number.ParseUInt64(s, NumberStyles.Integer, NumberFormatInfo.CurrentInfo);
    }

    public static ulong Parse(string s, NumberStyles style)
    {
        NumberFormatInfo.ValidateParseStyleInteger(style);
        if (s == null) ThrowHelper.ThrowArgumentNullException(ExceptionArgument.s);
        return Number.ParseUInt64(s, style, NumberFormatInfo.CurrentInfo);
    }

    public static ulong Parse(string s, IFormatProvider? provider)
    {
        if (s == null) ThrowHelper.ThrowArgumentNullException(ExceptionArgument.s);
        return Number.ParseUInt64(s, NumberStyles.Integer, NumberFormatInfo.GetInstance(provider));
    }

    public static ulong Parse(string s, NumberStyles style, IFormatProvider? provider)
    {
        NumberFormatInfo.ValidateParseStyleInteger(style);
        if (s == null) ThrowHelper.ThrowArgumentNullException(ExceptionArgument.s);
        return Number.ParseUInt64(s, style, NumberFormatInfo.GetInstance(provider));
    }

    public static ulong Parse(ReadOnlySpan<char> s, NumberStyles style = NumberStyles.Integer, IFormatProvider? provider = null)
    {
        NumberFormatInfo.ValidateParseStyleInteger(style);
        return Number.ParseUInt64(s, style, NumberFormatInfo.GetInstance(provider));
    }

    public static bool TryParse([NotNullWhen(true)] string? s, out ulong result)
    {
        if (s == null)
        {
            result = 0;
            return false;
        }

        return Number.TryParseUInt64IntegerStyle(s, NumberStyles.Integer, NumberFormatInfo.CurrentInfo, out result) == Number.ParsingStatus.OK;
    }

    public static bool TryParse(ReadOnlySpan<char> s, out ulong result)
    {
        return Number.TryParseUInt64IntegerStyle(s, NumberStyles.Integer, NumberFormatInfo.CurrentInfo, out result) == Number.ParsingStatus.OK;
    }

    public static bool TryParse([NotNullWhen(true)] string? s, NumberStyles style, IFormatProvider? provider, out ulong result)
    {
        NumberFormatInfo.ValidateParseStyleInteger(style);

        if (s == null)
        {
            result = 0;
            return false;
        }

        return Number.TryParseUInt64(s, style, NumberFormatInfo.GetInstance(provider), out result) == Number.ParsingStatus.OK;
    }

    public static bool TryParse(ReadOnlySpan<char> s, NumberStyles style, IFormatProvider? provider, out ulong result)
    {
        NumberFormatInfo.ValidateParseStyleInteger(style);
        return Number.TryParseUInt64(s, style, NumberFormatInfo.GetInstance(provider), out result) == Number.ParsingStatus.OK;
    }

}