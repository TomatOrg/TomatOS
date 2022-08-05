// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System.Runtime.CompilerServices;

namespace System;

#pragma warning disable SA1121 // explicitly using type aliases instead of built-in types

public readonly struct UIntPtr
{
    
    public static readonly UIntPtr Zero = new(0);
    
    public static int Size => Unsafe.SizeOf<nuint>();
    
    public static nuint MaxValue => (nuint)((1UL << Unsafe.SizeOf<nuint>() * 8) - 1);
    public static nuint MinValue => 0;
    
#pragma warning disable 169
    private readonly nuint _value;
#pragma warning restore 169
    
    public UIntPtr(uint value)
    {
        _value = value;
    }
    
    public UIntPtr(ulong value)
    {
        _value = (nuint)value;
    }
    
    public unsafe UIntPtr(void* value)
    {
        _value = (nuint)value;
    }

    public override bool Equals(object obj)
    {
        if (obj is UIntPtr value)
        {
            return _value == value._value;
        }
        return false;
    }

    public override int GetHashCode()
    {
        ulong value = _value;
        return value.GetHashCode();
    }

    // public uint ToUInt32()
    // {
    //     return checked((uint)_value);
    // }

    public ulong ToUInt64()
    {
        return _value;
    }

    public unsafe void* ToPointer()
    {
        return (void*)_value;   
    }

}