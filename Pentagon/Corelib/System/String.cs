using System.Collections;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace System;

[StructLayout(LayoutKind.Sequential)]
public class String : IEnumerable<char>
{

    public static readonly string Empty = "";

    private readonly int _length;

    public int Length => _length;

    [IndexerName("Chars")]
    public char this[int index]
    {
        
        get
        {
            if ((uint)index > (uint)Length) throw new IndexOutOfRangeException();
            return GetCharInternal(index);
        }
    }

    [MethodImpl(MethodImplOptions.InternalCall | MethodImplOptions.AggressiveInlining)]
    private extern char GetCharInternal(int index);
    
    public CharEnumerator GetEnumerator()
    {
        return new CharEnumerator(this);
    }
    
    IEnumerator IEnumerable.GetEnumerator()
    {
        return GetEnumerator();
    }
    
    IEnumerator<char> IEnumerable<char>.GetEnumerator()
    {
        return new CharEnumerator(this);
    }

    public override string ToString()
    {
        return this;
    }

    public override int GetHashCode()
    {
        return base.GetHashCode();
    }
    
    public static bool IsNullOrEmpty(string value)
    {
        return value == null || value.Length == 0;
    }
}