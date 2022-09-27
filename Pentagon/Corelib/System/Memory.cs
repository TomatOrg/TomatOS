using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace System;

[StructLayout(LayoutKind.Sequential)]
public readonly unsafe struct Memory<T>
{

    /// <summary>
    /// Returns an empty <see cref="Memory{T}"/>
    /// </summary>
    public static Memory<T> Empty => default;

    internal readonly object _object;
    internal readonly void* _ptr;
    private readonly int _length;
    
    /// <summary>
    /// The number of items in the memory.
    /// </summary>
    public int Length => _length;
    
    /// <summary>
    /// Returns true if Length is 0.
    /// </summary>
    public bool IsEmpty => _length == 0;

    /// <summary>
    /// Returns a span from the memory.
    /// </summary>
    public Span<T> Span => new(_ptr, _length);

    internal Memory(object obj, void* ptr, int length)
    {
        _object = obj;
        _ptr = ptr;
        _length = length;
    }

    /// <summary>
    /// Creates a new memory over the entirety of the target array.
    /// </summary>
    /// <param name="array">The target array.</param>
    /// <remarks>Returns default when <paramref name="array"/> is null.</remarks>
    /// <exception cref="System.ArrayTypeMismatchException">Thrown when <paramref name="array"/> is covariant and array's type is not exactly T[].</exception>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public Memory(T[]? array)
    {
        if (array == null)
        {
            this = default;
            return; // returns default
        }
        
        _object = array;
        _ptr = array.GetDataPtr();
        _length = array.Length;
    }
    
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal Memory(T[]? array, int start)
    {
        if (array == null)
        {
            if (start != 0)
                ThrowHelper.ThrowArgumentOutOfRangeException();
            this = default;
            return; // returns default
        }

        if ((uint)start > (uint)array.Length)
            ThrowHelper.ThrowArgumentOutOfRangeException();

        _object = array;
        _ptr = Unsafe.Add<T>(array.GetDataPtr(), start);
        _length = array.Length - start;
    }

    /// <summary>
    /// Creates a new memory over the portion of the target array beginning
    /// at 'start' index and ending at 'end' index (exclusive).
    /// </summary>
    /// <param name="array">The target array.</param>
    /// <param name="start">The index at which to begin the memory.</param>
    /// <param name="length">The number of items in the memory.</param>
    /// <remarks>Returns default when <paramref name="array"/> is null.</remarks>
    /// <exception cref="System.ArrayTypeMismatchException">Thrown when <paramref name="array"/> is covariant and array's type is not exactly T[].</exception>
    /// <exception cref="System.ArgumentOutOfRangeException">
    /// Thrown when the specified <paramref name="start"/> or end index is not in the range (&lt;0 or &gt;Length).
    /// </exception>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public Memory(T[]? array, int start, int length)
    {
        if (array == null)
        {
            if (start != 0 || length != 0)
                ThrowHelper.ThrowArgumentOutOfRangeException();
            this = default;
            return; // returns default
        }

        // See comment in Span<T>.Slice for how this works.
        if ((ulong)(uint)start + (ulong)(uint)length > (ulong)(uint)array.Length)
            ThrowHelper.ThrowArgumentOutOfRangeException();

        _object = array;
        _ptr = Unsafe.Add<T>(array.GetDataPtr(), start);
        _length = length;
    }

    /// <summary>
    /// Forms a slice out of the given memory, beginning at 'start'.
    /// </summary>
    /// <param name="start">The index at which to begin this slice.</param>
    /// <exception cref="System.ArgumentOutOfRangeException">
    /// Thrown when the specified <paramref name="start"/> index is not in range (&lt;0 or &gt;Length).
    /// </exception>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public Memory<T> Slice(int start)
    {
        if ((uint)start > (uint)_length)
        {
            ThrowHelper.ThrowArgumentOutOfRangeException(ExceptionArgument.start);
        }

        // It is expected for _index + start to be negative if the memory is already pre-pinned.
        return new Memory<T>(_object, Unsafe.Add<T>(_ptr, start), _length - start);
    }
    
    /// <summary>
    /// Forms a slice out of the given memory, beginning at 'start', of given length
    /// </summary>
    /// <param name="start">The index at which to begin this slice.</param>
    /// <param name="length">The desired length for the slice (exclusive).</param>
    /// <exception cref="System.ArgumentOutOfRangeException">
    /// Thrown when the specified <paramref name="start"/> or end index is not in range (&lt;0 or &gt;Length).
    /// </exception>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public Memory<T> Slice(int start, int length)
    {
        // See comment in Span<T>.Slice for how this works.
        if ((ulong)(uint)start + (ulong)(uint)length > (ulong)(uint)_length)
            ThrowHelper.ThrowArgumentOutOfRangeException();

        // It is expected for _index + start to be negative if the memory is already pre-pinned.
        return new Memory<T>(_object, Unsafe.Add<T>(_ptr, start), length);
    }
    
    /// <summary>
    /// Copies the contents of the memory into the destination. If the source
    /// and destination overlap, this method behaves as if the original values are in
    /// a temporary location before the destination is overwritten.
    ///
    /// <param name="destination">The Memory to copy items into.</param>
    /// <exception cref="System.ArgumentException">
    /// Thrown when the destination is shorter than the source.
    /// </exception>
    /// </summary>
    public void CopyTo(Memory<T> destination) => Span.CopyTo(destination.Span);

    /// <summary>
    /// Copies the contents of the memory into the destination. If the source
    /// and destination overlap, this method behaves as if the original values are in
    /// a temporary location before the destination is overwritten.
    ///
    /// <returns>If the destination is shorter than the source, this method
    /// return false and no data is written to the destination.</returns>
    /// </summary>
    /// <param name="destination">The span to copy items into.</param>
    public bool TryCopyTo(Memory<T> destination) => Span.TryCopyTo(destination.Span);

    /// <summary>
    /// Copies the contents from the memory into a new array.  This heap
    /// allocates, so should generally be avoided, however it is sometimes
    /// necessary to bridge the gap with APIs written in terms of arrays.
    /// </summary>
    public T[] ToArray() => Span.ToArray();

    
    /// <summary>
    /// Returns true if the memory points to the same array and has the same length.  Note that
    /// this does *not* check to see if the *contents* are equal.
    /// </summary>
    public bool Equals(Memory<T> other)
    {
        return
            _object == other._object &&
            _ptr == other._ptr &&
            _length == other._length;
    }
    
}