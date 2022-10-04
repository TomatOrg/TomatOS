// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System.Diagnostics;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace System;

/// <summary>
/// Represents a contiguous region of memory, similar to <see cref="ReadOnlySpan{T}"/>.
/// Unlike <see cref="ReadOnlySpan{T}"/>, it is not a byref-like type.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public readonly unsafe struct ReadOnlyMemory<T>
{
    
    internal readonly object _object;
    internal readonly void* _pointer;
    private readonly int _length;

    /// <summary>
    /// Creates a new memory over the entirety of the target array.
    /// </summary>
    /// <param name="array">The target array.</param>
    /// <remarks>Returns default when <paramref name="array"/> is null.</remarks>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public ReadOnlyMemory(T[] array)
    {
        if (array == null)
        {
            this = default;
            return; // returns default
        }

        _object = array;
        _pointer = array.GetDataPtr();
        _length = array.Length;
    }
    
    /// <summary>
    /// Creates a new memory over the portion of the target array beginning
    /// at 'start' index and ending at 'end' index (exclusive).
    /// </summary>
    /// <param name="array">The target array.</param>
    /// <param name="start">The index at which to begin the memory.</param>
    /// <param name="length">The number of items in the memory.</param>
    /// <remarks>Returns default when <paramref name="array"/> is null.</remarks>
    /// <exception cref="System.ArgumentOutOfRangeException">
    /// Thrown when the specified <paramref name="start"/> or end index is not in the range (&lt;0 or &gt;Length).
    /// </exception>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public ReadOnlyMemory(T[] array, int start, int length)
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
        _pointer = Unsafe.Add<T>(array.GetDataPtr(), start);
        _length = length;
    }
    
    /// <summary>Creates a new memory over the existing object, start, and length. No validation is performed.</summary>
    /// <param name="obj">The target object.</param>
    /// <param name="pointer">The pointer at which to begin the memory.</param>
    /// <param name="length">The number of items in the memory.</param>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal ReadOnlyMemory(object obj, void* pointer, int length)
    {
        _object = obj;
        _pointer = pointer;
        _length = length;
    }
    
    // Constructor for internal use only.
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal ReadOnlyMemory(object obj, ref T ptr, int length)
    {
        Debug.Assert(length >= 0);

        _object = obj;
        _pointer = Unsafe.AsPointer(ref ptr);
        _length = length;
    }

    /// <summary>
    /// Defines an implicit conversion of an array to a <see cref="ReadOnlyMemory{T}"/>
    /// </summary>
    public static implicit operator ReadOnlyMemory<T>(T[]? array) => new ReadOnlyMemory<T>(array);

    // /// <summary>
    // /// Defines an implicit conversion of a <see cref="ArraySegment{T}"/> to a <see cref="ReadOnlyMemory{T}"/>
    // /// </summary>
    // public static implicit operator ReadOnlyMemory<T>(ArraySegment<T> segment) => new ReadOnlyMemory<T>(segment.Array, segment.Offset, segment.Count);

    /// <summary>
    /// Returns an empty <see cref="ReadOnlyMemory{T}"/>
    /// </summary>
    public static ReadOnlyMemory<T> Empty => default;

    /// <summary>
    /// The number of items in the memory.
    /// </summary>
    public int Length => _length;

    /// <summary>
    /// Returns true if Length is 0.
    /// </summary>
    public bool IsEmpty => _length == 0;

    // /// <summary>
    // /// For <see cref="ReadOnlyMemory{Char}"/>, returns a new instance of string that represents the characters pointed to by the memory.
    // /// Otherwise, returns a <see cref="string"/> with the name of the type and the number of elements.
    // /// </summary>
    // public override string ToString()
    // {
    //     if (typeof(T) == typeof(char))
    //     {
    //         return (_object is string str) ? str.Substring(_index, _length) : Span.ToString();
    //     }
    //     return $"System.ReadOnlyMemory<{typeof(T).Name}>[{_length}]";
    // }
    
    /// <summary>
    /// Forms a slice out of the given memory, beginning at 'start'.
    /// </summary>
    /// <param name="start">The index at which to begin this slice.</param>
    /// <exception cref="System.ArgumentOutOfRangeException">
    /// Thrown when the specified <paramref name="start"/> index is not in range (&lt;0 or &gt;Length).
    /// </exception>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public ReadOnlyMemory<T> Slice(int start)
    {
        if ((uint)start > (uint)_length)
        {
            ThrowHelper.ThrowArgumentOutOfRangeException(ExceptionArgument.start);
        }

        // It is expected for _index + start to be negative if the memory is already pre-pinned.
        return new ReadOnlyMemory<T>(_object, Unsafe.Add<T>(_pointer, start), _length - start);
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
    public ReadOnlyMemory<T> Slice(int start, int length)
    {
        // See comment in Span<T>.Slice for how this works.
        if ((ulong)(uint)start + (ulong)(uint)length > (ulong)(uint)_length)
            ThrowHelper.ThrowArgumentOutOfRangeException(ExceptionArgument.start);

        // It is expected for _index + start to be negative if the memory is already pre-pinned.
        return new ReadOnlyMemory<T>(_object, Unsafe.Add<T>(_pointer, start), length);
    }

    /// <summary>
    /// Returns a span from the memory.
    /// </summary>
    public ReadOnlySpan<T> Span
    {
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        get => new(_pointer, _length);
    }
    
    /// <summary>
    /// Copies the contents of the read-only memory into the destination. If the source
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
    /// Copies the contents of the readonly-only memory into the destination. If the source
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

}