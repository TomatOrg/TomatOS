using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace System;

[StructLayout(LayoutKind.Sequential)]
public readonly ref struct Span<T>
{

    public static Span<T> Empty => new();

    private readonly ulong _ptr;
    private readonly int _length;

    public bool IsEmpty => _length == 0;
    public int Length => _length;

    internal Span(ulong ptr, int length)
    {
        _ptr = ptr;
        _length = length;
    }
    
    public Span(T[] array)
    {
        if (array == null)
        {
            this = default;
        }
        else
        {
            _ptr = array.GetDataPtr();
            _length = array.Length;
        }
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private extern ref T GetItemInternal(int index);
    
    public ref T this[int index]
    {
        get
        {
            if ((uint)index > (uint)_length)
                throw new IndexOutOfRangeException();
            return ref GetItemInternal(index);
        }
    }

    //
    // TODO: all of these can be optimized with native code, generated specifically
    //       for each of the types, this can then be used with arrays to make some 
    //       fast clear/copyto/fill and whatever else
    //
    
    public void Clear()
    {
        for (var i = 0; i < _length; i++)
        {
            this[i] = default;
        }
    }
    
    public void CopyTo(Span<T> destination)
    {
        if (!TryCopyTo(destination))
        {
            throw new ArgumentException("Destination is too short.", nameof(destination));
        }
    }

    public bool TryCopyTo(Span<T> destination)
    {
        if (destination.Length <= Length)
        {
            return false;
        }

        if (_ptr < destination._ptr && destination._ptr < _ptr + (ulong)_length * (ulong)typeof(T).StackSize)
        {
            for (var i = _length - 1; i >= 0; i--)
            {
                destination[i] = this[i];
            }
        }
        else
        {
            for (var i = 0; i < _length; i++)
            {
                destination[i] = this[i];
            }
        }

        return true;
    }

    public void Fill(T value)
    {
        for (var i = 0; i < _length; i++)
        {
            this[i] = value;
        }
    }

    public Span<T> Slice(int start)
    {
        if ((uint)start > (uint)Length)
            throw new ArgumentOutOfRangeException();
        
        return new Span<T>(_ptr + (ulong)typeof(T).StackSize * (ulong)start, Length - start);
    }

    public Span<T> Slice(int start, int length)
    {
        if ((ulong)(uint)start + (ulong)(uint)length > (ulong)(uint)_length)
            throw new ArgumentOutOfRangeException();

        return new Span<T>(_ptr + (ulong)typeof(T).StackSize * (ulong)start, length);
    }

    public T[] ToArray()
    {
        if (IsEmpty)
        {
            return Array.Empty<T>();
        }

        var arr = new T[Length];
        var span = new Span<T>(arr);
        CopyTo(span);
        return arr;
    }

    public static bool operator ==(Span<T> left, Span<T> right)
    {
        return left._ptr == right._ptr && left.Length == right.Length;
    }

    public static bool operator !=(Span<T> left, Span<T> right)
    {
        return left._ptr != right._ptr || left.Length != right.Length;
    }

    public static implicit operator Span<T>(T[] array)
    {
        return new Span<T>(array);
    }

    #region Enumerator

    public ref struct Enumerator
    {

        private Span<T> _span;
        private int _index = -1;

        public ref T Current => ref _span[_index];

        private Enumerator(Span<T> span)
        {
            _span = span;
        }
        
        public bool MoveNext()
        {
            var index = _index + 1;
            if (index >= _span.Length) 
                return false;
            
            _index = index;
            return true;

        }
        
    }

    #endregion
    
}