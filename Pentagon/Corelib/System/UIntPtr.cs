// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System.Runtime.CompilerServices;

namespace System;

using nuint_t = System.UInt64;

public readonly struct UIntPtr
{
    public static readonly UIntPtr Zero;
    
    public static int Size => sizeof(nuint_t);

    public static UIntPtr MaxValue => new(nuint_t.MaxValue);

    public static UIntPtr MinValue => new(nuint_t.MinValue);

    private readonly unsafe void* _value;

    public unsafe UIntPtr(uint value)
    {
        _value = (void*)value;
    }
    
    public unsafe UIntPtr(ulong value)
    {
        _value = (void*)value;
    }
    
    public unsafe UIntPtr(void* value)
    {
        _value = value;
    }
    
    public override unsafe bool Equals(object obj)
    {
        if (obj is UIntPtr ptr)
        {
            return _value == ptr._value;
        }
        return false;
    }
    
    
    public override unsafe int GetHashCode()
    {
        var l = (ulong)_value;
        return unchecked((int)l) ^ (int)(l >> 32);
    }
    
    public unsafe void* ToPointer() => _value;

    public unsafe uint ToUInt32()
    {
        return /*checked*/((uint)_value);
    }

    public unsafe ulong ToUInt64() => (ulong)_value;

    // public static explicit operator UIntPtr(uint value) =>
    //     new(value);
    //
    // public static explicit operator UIntPtr(ulong value) =>
    //     new(value);
    //
    // public static unsafe explicit operator UIntPtr(void* value) =>
    //     new(value);
    //
    // public static unsafe explicit operator void*(UIntPtr value) =>
    //     (void*)(nuint)value;
    //
    // public static explicit operator uint(UIntPtr value) =>
    //     /*checked*/((uint)(nuint)value);
    //
    // public static explicit operator ulong(UIntPtr value) =>
    //     (nuint)value;
    //
    // public static bool operator ==(UIntPtr value1, UIntPtr value2) =>
    //     (nuint)value1 == (nuint)value2;
    //
    // public static bool operator !=(UIntPtr value1, UIntPtr value2) =>
    //     (nuint)value1 != (nuint)value2;
    //
    // public static UIntPtr Add(UIntPtr pointer, int offset) =>
    //     pointer + offset;
    //
    // public static UIntPtr operator +(UIntPtr pointer, int offset) =>
    //     (nuint)pointer + (nuint)offset;
    //
    // public static UIntPtr Subtract(UIntPtr pointer, int offset) =>
    //     pointer - offset;
    //
    // public static UIntPtr operator -(UIntPtr pointer, int offset) =>
    //     (nuint)pointer - (nuint)offset;
    
}