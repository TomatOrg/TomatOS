using System.Runtime.InteropServices;

namespace System;

[StructLayout(LayoutKind.Sequential)]
public class Array
{

    private readonly int _length;

    public int Length => _length;
    public long LongLength => _length;
    public int Rank => 1;

}