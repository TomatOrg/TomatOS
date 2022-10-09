using System.Diagnostics.CodeAnalysis;
using System.Globalization;

namespace System;

public readonly struct SByte : ISpanFormattable, IComparable<sbyte>, IEquatable<sbyte>
{
    private readonly sbyte m_value;

    // The maximum value that a Byte may represent: 127.
    public const sbyte MaxValue = (sbyte)0x7F;

    // The minimum value that a Byte may represent: -128.
    public const sbyte MinValue = unchecked((sbyte)0x80);

    public int CompareTo(sbyte value)
    {
        return m_value - value;
    }

    // Determines whether two Byte objects are equal.
    public override bool Equals([NotNullWhen(true)] object? obj)
    {
        if (obj is not sbyte @sbyte)
        {
            return false;
        }
        return m_value == @sbyte.m_value;
    }

    public bool Equals(sbyte obj)
    {
        return m_value == obj;
    }

    // Gets a hash code for this instance.
    public override int GetHashCode()
    {
        return m_value;
    }


    // Provides a string representation of a byte.
    public override string ToString()
    {
        return Number.Int32ToDecStr(m_value);
    }

    public string ToString(string? format)
    {
        return ToString(format, null);
    }

    public string ToString(IFormatProvider? provider)
    {
        return Number.FormatInt32(m_value, 0, null, provider);
    }

    public string ToString(string? format, IFormatProvider? provider)
    {
        return Number.FormatInt32(m_value, 0x000000FF, format, provider);
    }

    public bool TryFormat(Span<char> destination, out int charsWritten, ReadOnlySpan<char> format = default, IFormatProvider? provider = null)
    {
        return Number.TryFormatInt32(m_value, 0x000000FF, format, provider, destination, out charsWritten);
    }

    public static sbyte Parse(string s)
    {
        if (s == null) ThrowHelper.ThrowArgumentNullException(ExceptionArgument.s);
        return Parse((ReadOnlySpan<char>)s, NumberStyles.Integer, NumberFormatInfo.CurrentInfo);
    }

    public static sbyte Parse(string s, NumberStyles style)
    {
        NumberFormatInfo.ValidateParseStyleInteger(style);
        if (s == null) ThrowHelper.ThrowArgumentNullException(ExceptionArgument.s);
        return Parse((ReadOnlySpan<char>)s, style, NumberFormatInfo.CurrentInfo);
    }

    public static sbyte Parse(string s, IFormatProvider? provider)
    {
        if (s == null) ThrowHelper.ThrowArgumentNullException(ExceptionArgument.s);
        return Parse((ReadOnlySpan<char>)s, NumberStyles.Integer, NumberFormatInfo.GetInstance(provider));
    }

    // Parses a signed byte from a String in the given style.  If
    // a NumberFormatInfo isn't specified, the current culture's
    // NumberFormatInfo is assumed.
    //
    public static sbyte Parse(string s, NumberStyles style, IFormatProvider? provider)
    {
        NumberFormatInfo.ValidateParseStyleInteger(style);
        if (s == null) ThrowHelper.ThrowArgumentNullException(ExceptionArgument.s);
        return Parse((ReadOnlySpan<char>)s, style, NumberFormatInfo.GetInstance(provider));
    }

    public static sbyte Parse(ReadOnlySpan<char> s, NumberStyles style = NumberStyles.Integer, IFormatProvider? provider = null)
    {
        NumberFormatInfo.ValidateParseStyleInteger(style);
        return Parse(s, style, NumberFormatInfo.GetInstance(provider));
    }

    private static sbyte Parse(ReadOnlySpan<char> s, NumberStyles style, NumberFormatInfo info)
    {
        Number.ParsingStatus status = Number.TryParseInt32(s, style, info, out int i);
        if (status != Number.ParsingStatus.OK)
        {
            Number.ThrowOverflowOrFormatException(status, TypeCode.SByte);
        }

        // For hex number styles AllowHexSpecifier >> 2 == 0x80 and cancels out MinValue so the check is effectively: (uint)i > byte.MaxValue
        // For integer styles it's zero and the effective check is (uint)(i - MinValue) > byte.MaxValue
        if ((uint)(i - MinValue - ((int)(style & NumberStyles.AllowHexSpecifier) >> 2)) > byte.MaxValue)
        {
            Number.ThrowOverflowException(TypeCode.SByte);
        }
        return (sbyte)i;
    }

    public static bool TryParse([NotNullWhen(true)] string? s, out sbyte result)
    {
        if (s == null)
        {
            result = 0;
            return false;
        }

        return TryParse((ReadOnlySpan<char>)s, NumberStyles.Integer, NumberFormatInfo.CurrentInfo, out result);
    }

    public static bool TryParse(ReadOnlySpan<char> s, out sbyte result)
    {
        return TryParse(s, NumberStyles.Integer, NumberFormatInfo.CurrentInfo, out result);
    }

    public static bool TryParse([NotNullWhen(true)] string? s, NumberStyles style, IFormatProvider? provider, out sbyte result)
    {
        NumberFormatInfo.ValidateParseStyleInteger(style);

        if (s == null)
        {
            result = 0;
            return false;
        }

        return TryParse((ReadOnlySpan<char>)s, style, NumberFormatInfo.GetInstance(provider), out result);
    }

    public static bool TryParse(ReadOnlySpan<char> s, NumberStyles style, IFormatProvider? provider, out sbyte result)
    {
        NumberFormatInfo.ValidateParseStyleInteger(style);
        return TryParse(s, style, NumberFormatInfo.GetInstance(provider), out result);
    }

    private static bool TryParse(ReadOnlySpan<char> s, NumberStyles style, NumberFormatInfo info, out sbyte result)
    {
        // For hex number styles AllowHexSpecifier >> 2 == 0x80 and cancels out MinValue so the check is effectively: (uint)i > byte.MaxValue
        // For integer styles it's zero and the effective check is (uint)(i - MinValue) > byte.MaxValue
        if (Number.TryParseInt32(s, style, info, out int i) != Number.ParsingStatus.OK
            || (uint)(i - MinValue - ((int)(style & NumberStyles.AllowHexSpecifier) >> 2)) > byte.MaxValue)
        {
            result = 0;
            return false;
        }
        result = (sbyte)i;
        return true;
    }

}