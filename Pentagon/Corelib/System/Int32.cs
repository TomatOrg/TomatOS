// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System.Diagnostics.CodeAnalysis;
using System.Globalization;

namespace System;

public readonly struct Int32 : ISpanFormattable, IComparable<int>, IEquatable<int>
{
    
    private readonly int _value;

    public const int MaxValue = 0x7fffffff;
    public const int MinValue = unchecked((int)0x80000000);

    public int CompareTo(int value)
    {
        // NOTE: Cannot use return (_value - value) as this causes a wrap
        // around in cases where _value - value > MaxValue.
        if (_value < value) return -1;
        if (_value > value) return 1;
        return 0;
    }

    public override bool Equals([NotNullWhen(true)] object? obj)
    {
        if (obj is not int i)
        {
            return false;
        }
        return _value == i._value;
    }

    public bool Equals(int obj)
    {
        return _value == obj;
    }

    // The absolute value of the int contained.
    public override int GetHashCode()
    {
        return _value;
    }

    public override string ToString()
    {
        return Number.Int32ToDecStr(_value);
    }

    public string ToString(string? format)
    {
        return ToString(format, null);
    }

    public string ToString(IFormatProvider? provider)
    {
        return Number.FormatInt32(_value, 0, null, provider);
    }

    public string ToString(string? format, IFormatProvider? provider)
    {
        return Number.FormatInt32(_value, ~0, format, provider);
    }

    public bool TryFormat(Span<char> destination, out int charsWritten, ReadOnlySpan<char> format = default, IFormatProvider? provider = null)
    {
        return Number.TryFormatInt32(_value, ~0, format, provider, destination, out charsWritten);
    }

    public static int Parse(string s)
    {
        if (s == null) ThrowHelper.ThrowArgumentNullException(ExceptionArgument.s);
        return Number.ParseInt32(s, NumberStyles.Integer, NumberFormatInfo.CurrentInfo);
    }

    public static int Parse(string s, NumberStyles style)
    {
        NumberFormatInfo.ValidateParseStyleInteger(style);
        if (s == null) ThrowHelper.ThrowArgumentNullException(ExceptionArgument.s);
        return Number.ParseInt32(s, style, NumberFormatInfo.CurrentInfo);
    }

    // Parses an integer from a String in the given style.  If
    // a NumberFormatInfo isn't specified, the current culture's
    // NumberFormatInfo is assumed.
    //
    public static int Parse(string s, IFormatProvider? provider)
    {
        if (s == null) ThrowHelper.ThrowArgumentNullException(ExceptionArgument.s);
        return Number.ParseInt32(s, NumberStyles.Integer, NumberFormatInfo.GetInstance(provider));
    }

    // Parses an integer from a String in the given style.  If
    // a NumberFormatInfo isn't specified, the current culture's
    // NumberFormatInfo is assumed.
    //
    public static int Parse(string s, NumberStyles style, IFormatProvider? provider)
    {
        NumberFormatInfo.ValidateParseStyleInteger(style);
        if (s == null) ThrowHelper.ThrowArgumentNullException(ExceptionArgument.s);
        return Number.ParseInt32(s, style, NumberFormatInfo.GetInstance(provider));
    }

    public static int Parse(ReadOnlySpan<char> s, NumberStyles style = NumberStyles.Integer, IFormatProvider? provider = null)
    {
        NumberFormatInfo.ValidateParseStyleInteger(style);
        return Number.ParseInt32(s, style, NumberFormatInfo.GetInstance(provider));
    }

    // Parses an integer from a String. Returns false rather
    // than throwing an exception if input is invalid.
    //
    public static bool TryParse([NotNullWhen(true)] string? s, out int result)
    {
        if (s == null)
        {
            result = 0;
            return false;
        }

        return Number.TryParseInt32IntegerStyle(s, NumberStyles.Integer, NumberFormatInfo.CurrentInfo, out result) == Number.ParsingStatus.OK;
    }

    public static bool TryParse(ReadOnlySpan<char> s, out int result)
    {
        return Number.TryParseInt32IntegerStyle(s, NumberStyles.Integer, NumberFormatInfo.CurrentInfo, out result) == Number.ParsingStatus.OK;
    }

    // Parses an integer from a String in the given style. Returns false rather
    // than throwing an exception if input is invalid.
    //
    public static bool TryParse([NotNullWhen(true)] string? s, NumberStyles style, IFormatProvider? provider, out int result)
    {
        NumberFormatInfo.ValidateParseStyleInteger(style);

        if (s == null)
        {
            result = 0;
            return false;
        }

        return Number.TryParseInt32(s, style, NumberFormatInfo.GetInstance(provider), out result) == Number.ParsingStatus.OK;
    }

    public static bool TryParse(ReadOnlySpan<char> s, NumberStyles style, IFormatProvider? provider, out int result)
    {
        NumberFormatInfo.ValidateParseStyleInteger(style);
        return Number.TryParseInt32(s, style, NumberFormatInfo.GetInstance(provider), out result) == Number.ParsingStatus.OK;
    }

}