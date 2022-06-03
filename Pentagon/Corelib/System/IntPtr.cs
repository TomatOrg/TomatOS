namespace System;

public readonly struct IntPtr
{
#pragma warning disable 169
    private readonly unsafe void* _value;
#pragma warning restore 169
}