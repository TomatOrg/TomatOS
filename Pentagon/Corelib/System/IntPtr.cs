// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

namespace System;

using nint_t = System.Int64;

/// <summary>
/// A platform-specific type that is used to represent a pointer or a handle.
/// </summary>
public readonly struct IntPtr
{

    public static readonly IntPtr Zero;

    public static IntPtr MaxValue => new(nint_t.MaxValue);

    public static IntPtr MinValue => new(nint_t.MinValue);

    public static int Size => sizeof(nint_t);

    private readonly unsafe void* _value;

    public unsafe IntPtr(int value)
    {
        _value = (void*)value;
    }

    public unsafe IntPtr(long value)
    {
        _value = (void*)value;
    }
    
    public unsafe IntPtr(void* value)
    {
        _value = value;
    }
    
    public override unsafe bool Equals(object obj) =>
        obj is IntPtr other &&
        _value == other._value;

    public override unsafe int GetHashCode()
    {
        var l = (long)_value;
        return unchecked((int)l) ^ (int)(l >> 32);
    }
        
    public unsafe void* ToPointer() => _value;

    public unsafe int ToInt32()
    {
        var l = (long)_value;
        return /*checked*/((int)l);
    }
    
    public unsafe long ToInt64() =>
        (nint)_value;
    
    // public static explicit operator IntPtr(int value) =>
    //     new(value);
    //
    // public static explicit operator IntPtr(long value) =>
    //     new(value);
    //
    // public static unsafe explicit operator IntPtr(void* value) =>
    //     new(value);
    //
    // public static unsafe explicit operator void*(IntPtr value) =>
    //     (void*)(nint)value;
    //
    // public static explicit operator int(IntPtr value)
    // {
    //     var l = (long)value;
    //     return /*checked*/((int)l);
    // }
    //
    // public static explicit operator long(IntPtr value) =>
    //     (nint)value;
    //
    // public static bool operator ==(IntPtr value1, IntPtr value2) =>
    //     (nint)value1 == (nint)value2;
    //
    // public static bool operator !=(IntPtr value1, IntPtr value2) =>
    //     (nint)value1 != (nint)value2;
    //
    // public static IntPtr Add(IntPtr pointer, int offset) =>
    //     pointer + offset;
    //
    // public static IntPtr operator +(IntPtr pointer, int offset) =>
    //     (nint)pointer + (nint)offset;
    //
    // public static IntPtr Subtract(IntPtr pointer, int offset) =>
    //     pointer - offset;
    //
    // public static IntPtr operator -(IntPtr pointer, int offset) =>
    //     (nint)pointer - (nint)offset;

}