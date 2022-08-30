// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System.Collections;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace System;

// The String class represents a static string of characters.  Many of
// the string methods perform some type of transformation on the current
// instance and return the result as a new string.  As with arrays, character
// positions (indices) are zero-based.
[StructLayout(LayoutKind.Sequential)]
public class String : IEnumerable<char>
{
    
    public static readonly string Empty = "";

    private readonly int _length;
    private char _firstChar;

    public int Length => _length;

    [IndexerName("Chars")]
    public char this[int index]
    {
        get
        {
            if ((uint)index > (uint)Length) throw new IndexOutOfRangeException();
            return Unsafe.Add(ref _firstChar, index);
        }
    }
    
    internal ref char GetRawStringData() => ref _firstChar;

    private String(int length)
    {
        _length = length;
    }

    internal static string FastAllocateString(int length)
    {
        return new string(length);
    }
    
    public String(char[] chars)
    {
        _length = chars.Length;
        
        var span = new Span<char>(ref GetRawStringData(), Length);
        chars.AsSpan().CopyTo(span);
    }

    public String(char[] chars, int startIndex, int length)
    {
        _length = length;

        var span = new Span<char>(ref GetRawStringData(), Length);
        chars.AsSpan(startIndex, length).CopyTo(span);
    }

    #region Concat

    public static string Concat(object arg0)
    {
        return arg0?.ToString() ?? Empty;
    }

    public static string Concat(object arg0, object arg1)
    {
        return Concat(Concat(arg0), Concat(arg1));
    }

    public static string Concat(object arg0, object arg1, object arg2)
    {
        return Concat(Concat(arg0), Concat(arg1), Concat(arg2));
    }

    public static string Concat(string arg0, string arg1)
    {
        var str = new string(arg0.Length + arg1.Length);
        var span = new Span<char>(ref str.GetRawStringData(), str.Length);
        arg0.AsSpan().CopyTo(span);
        arg1.AsSpan().CopyTo(span.Slice(arg0.Length));
        return str;
    }

    public static string Concat(string arg0, string arg1, string arg2)
    {
        var str = new string(arg0.Length + arg1.Length + arg2.Length);
        var span = new Span<char>(ref str.GetRawStringData(), str.Length);
        arg0.AsSpan().CopyTo(span);
        arg1.AsSpan().CopyTo(span.Slice(arg0.Length));
        arg2.AsSpan().CopyTo(span.Slice(arg0.Length + arg1.Length));
        return str;
    }

    public static string Concat(string arg0, string arg1, string arg2, string arg3)
    {
        var str = new string(arg0.Length + arg1.Length + arg2.Length + arg3.Length);
        var span = new Span<char>(ref str.GetRawStringData(), str.Length);
        arg0.AsSpan().CopyTo(span);
        arg1.AsSpan().CopyTo(span.Slice(arg0.Length));
        arg2.AsSpan().CopyTo(span.Slice(arg0.Length + arg1.Length));
        arg3.AsSpan().CopyTo(span.Slice(arg0.Length + arg1.Length + arg2.Length));
        return str;
    }

    public static string Concat(params object[] values)
    {
        var strs = new string[values.Length];
        for (var i = 0; i < values.Length; i++)
        {
            strs[i] = Concat(values[i]);
        }
        return Concat(strs);
    }
    
    public static string Concat(params string[] values)
    {
        if (values == null)
            throw new ArgumentNullException(nameof(values));

        // calculate the total length
        var len = 0;
        foreach (var val in values)
        {
            len += val.Length;
        }

        // allocate it 
        var str = new string(len);
        var span = new Span<char>(ref str.GetRawStringData(), str.Length);
        
        // copy it 
        var off = 0;
        foreach (var val in values)
        {
            val.AsSpan().CopyTo(span.Slice(off));
            off += val.Length;
        }

        return str;
    }
    
    #endregion
    
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