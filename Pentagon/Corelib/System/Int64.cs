// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

namespace System;

public readonly struct Int64
{
    
    public const long MaxValue = 9223372036854775807;
    public const long MinValue = -9223372036854775808;
    
#pragma warning disable 169
    private readonly long _value;
#pragma warning restore 169

    public override bool Equals(object obj)
    {
        if (obj is long value)
        {
            return _value == value;
        }
        return false;
    }

    public override int GetHashCode()
    {
        return (int)_value ^ (int)(_value >> 32);
    }

    public override string ToString()
    {
        return "";
    }
    
}