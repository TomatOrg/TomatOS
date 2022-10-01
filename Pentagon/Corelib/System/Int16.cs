// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System.Diagnostics.CodeAnalysis;
using System.Globalization;

namespace System;

public readonly struct Int16 : ISpanFormattable, IComparable<short>, IEquatable<short>
{

    private readonly short m_value;

    public const short MaxValue = (short)0x7FFF;
    public const short MinValue = unchecked((short)0x8000);

    public int CompareTo(short value)
    {
        return m_value - value;
    }

    public override bool Equals([NotNullWhen(true)] object? obj)
    {
        if (obj is not short s)
        {
            return false;
        }
        return m_value == s.m_value;
    }

    public bool Equals(short obj)
    {
        return m_value == obj;
    }
   
    // Returns a HashCode for the Int16
    public override int GetHashCode()
    {
        return m_value;
    }

    public override string ToString()
    {
        return Number.Int32ToDecStr(m_value);
    }

    public string ToString(IFormatProvider? provider)
    {
        return Number.FormatInt32(m_value, 0, null, provider);
    }

    public string ToString(string? format)
    {
        return ToString(format, null);
    }

    public string ToString(string? format, IFormatProvider? provider)
    {
        return Number.FormatInt32(m_value, 0x0000FFFF, format, provider);
    }

    public bool TryFormat(Span<char> destination, out int charsWritten, ReadOnlySpan<char> format = default, IFormatProvider? provider = null)
    {
        return Number.TryFormatInt32(m_value, 0x0000FFFF, format, provider, destination, out charsWritten);
    }

    public static short Parse(string s)
    {
        if (s == null) ThrowHelper.ThrowArgumentNullException(ExceptionArgument.s);
        return Parse((ReadOnlySpan<char>)s, NumberStyles.Integer, NumberFormatInfo.CurrentInfo);
    }

    public static short Parse(string s, NumberStyles style)
    {
        NumberFormatInfo.ValidateParseStyleInteger(style);
        if (s == null) ThrowHelper.ThrowArgumentNullException(ExceptionArgument.s);
        return Parse((ReadOnlySpan<char>)s, style, NumberFormatInfo.CurrentInfo);
    }

    public static short Parse(string s, IFormatProvider? provider)
    {
        if (s == null) ThrowHelper.ThrowArgumentNullException(ExceptionArgument.s);
        return Parse((ReadOnlySpan<char>)s, NumberStyles.Integer, NumberFormatInfo.GetInstance(provider));
    }

    public static short Parse(string s, NumberStyles style, IFormatProvider? provider)
    {
        NumberFormatInfo.ValidateParseStyleInteger(style);
        if (s == null) ThrowHelper.ThrowArgumentNullException(ExceptionArgument.s);
        return Parse((ReadOnlySpan<char>)s, style, NumberFormatInfo.GetInstance(provider));
    }

    public static short Parse(ReadOnlySpan<char> s, NumberStyles style = NumberStyles.Integer, IFormatProvider? provider = null)
    {
        NumberFormatInfo.ValidateParseStyleInteger(style);
        return Parse(s, style, NumberFormatInfo.GetInstance(provider));
    }

    private static short Parse(ReadOnlySpan<char> s, NumberStyles style, NumberFormatInfo info)
    {
        Number.ParsingStatus status = Number.TryParseInt32(s, style, info, out int i);
        if (status != Number.ParsingStatus.OK)
        {
            Number.ThrowOverflowOrFormatException(status, TypeCode.Int16);
        }

        // For hex number styles AllowHexSpecifier << 6 == 0x8000 and cancels out MinValue so the check is effectively: (uint)i > ushort.MaxValue
        // For integer styles it's zero and the effective check is (uint)(i - MinValue) > ushort.MaxValue
        if ((uint)(i - MinValue - ((int)(style & NumberStyles.AllowHexSpecifier) << 6)) > ushort.MaxValue)
        {
            Number.ThrowOverflowException(TypeCode.Int16);
        }
        return (short)i;
    }

    public static bool TryParse([NotNullWhen(true)] string? s, out short result)
    {
        if (s == null)
        {
            result = 0;
            return false;
        }

        return TryParse((ReadOnlySpan<char>)s, NumberStyles.Integer, NumberFormatInfo.CurrentInfo, out result);
    }

    public static bool TryParse(ReadOnlySpan<char> s, out short result)
    {
        return TryParse(s, NumberStyles.Integer, NumberFormatInfo.CurrentInfo, out result);
    }

    public static bool TryParse([NotNullWhen(true)] string? s, NumberStyles style, IFormatProvider? provider, out short result)
    {
        NumberFormatInfo.ValidateParseStyleInteger(style);

        if (s == null)
        {
            result = 0;
            return false;
        }

        return TryParse((ReadOnlySpan<char>)s, style, NumberFormatInfo.GetInstance(provider), out result);
    }

    public static bool TryParse(ReadOnlySpan<char> s, NumberStyles style, IFormatProvider? provider, out short result)
    {
        NumberFormatInfo.ValidateParseStyleInteger(style);
        return TryParse(s, style, NumberFormatInfo.GetInstance(provider), out result);
    }

    private static bool TryParse(ReadOnlySpan<char> s, NumberStyles style, NumberFormatInfo info, out short result)
    {
        // For hex number styles AllowHexSpecifier << 6 == 0x8000 and cancels out MinValue so the check is effectively: (uint)i > ushort.MaxValue
        // For integer styles it's zero and the effective check is (uint)(i - MinValue) > ushort.MaxValue
        if (Number.TryParseInt32(s, style, info, out int i) != Number.ParsingStatus.OK
            || (uint)(i - MinValue - ((int)(style & NumberStyles.AllowHexSpecifier) << 6)) > ushort.MaxValue)
        {
            result = 0;
            return false;
        }
        result = (short)i;
        return true;
    }

}