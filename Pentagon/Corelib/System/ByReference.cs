// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System.Runtime.CompilerServices;

namespace System;

// ByReference<T> is meant to be used to represent "ref T" fields. It is working
// around lack of first class support for byref fields in C# and IL. The JIT and
// type loader has special handling for it that turns it into a thin wrapper around ref T.
internal readonly unsafe ref struct ByReference<T>
{

    internal readonly void* _value;
    
    public ref T Value
    {
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        get => ref Unsafe.AsRef<T>(_value);
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public ByReference(ref T value)
    {
        _value = Unsafe.AsPointer(ref value);
    }

}