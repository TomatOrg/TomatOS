using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace System;

[StructLayout(LayoutKind.Sequential)]
public readonly struct Memory<T>
{

    public static Memory<T> Empty { get; } = new();

    internal readonly object _obj;
    internal readonly ulong _ptr;
    private readonly int _length;

    public bool IsEmpty => _length == 0;
    public int Length => _length;
    public Span<T> Span => new(_ptr, _length);

    internal Memory(object obj, ulong ptr, int length)
    {
        _obj = obj;
        _ptr = ptr;
        _length = length;
    }

    public Memory(T[] array)
    {
        if (array == null)
        {
            this = default;
        }
        else
        {
            _obj = array;
            _ptr = array.GetDataPtr();
            _length = array.Length;
        }
    }

    public Memory(T[] array, int start, int length)
    {
        if (array == null)
        {
            if (start != 0 || length != 0)
                throw new ArgumentOutOfRangeException();

            this = default;
        }
        else
        {
            this = new Memory<T>(array).Slice(start, length);
        }
    }

    public void CopyTo(Memory<T> destination)
    {
        Span.CopyTo(destination.Span);
    }

    public Memory<T> Slice(int start)
    {
        if ((uint)start > (uint)Length)
            throw new ArgumentOutOfRangeException();
        
        return new Memory<T>(_obj, _ptr + (ulong)Unsafe.SizeOf<T>() * (ulong)start, Length - start);
    }

    public Memory<T> Slice(int start, int length)
    {
        if ((ulong)(uint)start + (ulong)(uint)length > (ulong)(uint)_length)
            throw new ArgumentOutOfRangeException();

        return new Memory<T>(_obj, _ptr + (ulong)Unsafe.SizeOf<T>() * (ulong)start, length);
    }

    public T[] ToArray()
    {
        return Span.ToArray();
    }

    public bool TryCopyTo(Memory<T> destination)
    {
        return Span.TryCopyTo(destination.Span);
    }

    public static implicit operator Memory<T>(T[] array)
    {
        return new Memory<T>(array);
    }
    
}