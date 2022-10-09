// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System.Diagnostics;
using System.Drawing;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace System;

/// <summary>
/// ReadOnlySpan represents a contiguous region of arbitrary memory. Unlike arrays, it can point to either managed
/// or native memory, or to memory allocated on the stack. It is type- and memory-safe.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public readonly ref struct ReadOnlySpan<T>
{
    /// <summary>A byref or a native ptr.</summary>
    internal readonly ByReference<T> _pointer;
    /// <summary>The number of elements this ReadOnlySpan contains.</summary>
    private readonly int _length;

    /// <summary>
    /// Creates a new read-only span over the entirety of the target array.
    /// </summary>
    /// <param name="array">The target array.</param>
    /// <remarks>Returns default when <paramref name="array"/> is null.</remarks>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public ReadOnlySpan(T[] array)
    {
        if (array == null)
        {
            this = default;
            return; // returns default
        }

        _pointer = new ByReference<T>(ref MemoryMarshal.GetArrayDataReference(array));
        _length = array.Length;
    }
    
    /// <summary>
    /// Creates a new read-only span over the portion of the target array beginning
    /// at 'start' index and ending at 'end' index (exclusive).
    /// </summary>
    /// <param name="array">The target array.</param>
    /// <param name="start">The index at which to begin the read-only span.</param>
    /// <param name="length">The number of items in the read-only span.</param>
    /// <remarks>Returns default when <paramref name="array"/> is null.</remarks>
    /// <exception cref="System.ArgumentOutOfRangeException">
    /// Thrown when the specified <paramref name="start"/> or end index is not in the range (&lt;0 or &gt;Length).
    /// </exception>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public ReadOnlySpan(T[] array, int start, int length)
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

        _pointer = new ByReference<T>(ref Unsafe.Add(ref MemoryMarshal.GetArrayDataReference(array), (nint)(uint)start /* force zero-extension */));
        _length = length;
    }
    
    /// <summary>
    /// Creates a new read-only span over the target unmanaged buffer.  Clearly this
    /// is quite dangerous, because we are creating arbitrarily typed T's
    /// out of a void*-typed block of memory.  And the length is not checked.
    /// But if this creation is correct, then all subsequent uses are correct.
    /// </summary>
    /// <param name="pointer">An unmanaged pointer to memory.</param>
    /// <param name="length">The number of <typeparamref name="T"/> elements the memory contains.</param>
    /// <exception cref="System.ArgumentException">
    /// Thrown when <typeparamref name="T"/> is reference type or contains pointers and hence cannot be stored in unmanaged memory.
    /// </exception>
    /// <exception cref="System.ArgumentOutOfRangeException">
    /// Thrown when the specified <paramref name="length"/> is negative.
    /// </exception>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal unsafe ReadOnlySpan(void* pointer, int length)
    {
        if (length < 0)
            ThrowHelper.ThrowArgumentOutOfRangeException();

        _pointer = new ByReference<T>(ref Unsafe.AsRef<T>(pointer));
        _length = length;
    }
    
    // Constructor for internal use only.
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal ReadOnlySpan(ref T ptr, int length)
    {
        Debug.Assert(length >= 0);

        _pointer = new ByReference<T>(ref ptr);
        _length = length;
    }
    
    /// <summary>
    /// Returns the specified element of the read-only span.
    /// </summary>
    /// <param name="index"></param>
    /// <returns></returns>
    /// <exception cref="System.IndexOutOfRangeException">
    /// Thrown when index less than 0 or index greater than or equal to Length
    /// </exception>
    public ref readonly T this[int index]
    {
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        get
        {
            if ((uint)index >= (uint)_length)
                ThrowHelper.ThrowIndexOutOfRangeException();
            return ref Unsafe.Add(ref _pointer.Value, (nint)(uint)index /* force zero-extension */);
        }
    }

    /// <summary>
    /// The number of items in the read-only span.
    /// </summary>
    public int Length => _length;

    /// <summary>
    /// Returns true if Length is 0.
    /// </summary>
    public bool IsEmpty => 0 >= (uint)_length; // Workaround for https://github.com/dotnet/runtime/issues/10950
    
    /// <summary>
    /// Returns false if left and right point at the same memory and have the same length.  Note that
    /// this does *not* check to see if the *contents* are equal.
    /// </summary>
    public static bool operator !=(ReadOnlySpan<T> left, ReadOnlySpan<T> right) => !(left == right);

    /// <summary>
    /// This method is not supported as spans cannot be boxed. To compare two spans, use operator==.
    /// <exception cref="System.NotSupportedException">
    /// Always thrown by this method.
    /// </exception>
    /// </summary>
    [Obsolete("Equals() on ReadOnlySpan has will always throw an exception. Use the equality operator instead.")]
    public override bool Equals(object? obj) =>
        throw new NotSupportedException(NotSupportedException.CannotCallEqualsOnSpan);

    /// <summary>
    /// This method is not supported as spans cannot be boxed.
    /// <exception cref="System.NotSupportedException">
    /// Always thrown by this method.
    /// </exception>
    /// </summary>
    [Obsolete("GetHashCode() on ReadOnlySpan will always throw an exception.")]
    public override int GetHashCode() =>
        throw new NotSupportedException(NotSupportedException.CannotCallGetHashCodeOnSpan);

    /// <summary>
    /// Defines an implicit conversion of an array to a <see cref="ReadOnlySpan{T}"/>
    /// </summary>
    public static implicit operator ReadOnlySpan<T>(T[]? array) => new ReadOnlySpan<T>(array);

    // /// <summary>
    // /// Defines an implicit conversion of a <see cref="ArraySegment{T}"/> to a <see cref="ReadOnlySpan{T}"/>
    // /// </summary>
    // public static implicit operator ReadOnlySpan<T>(ArraySegment<T> segment)
    //     => new ReadOnlySpan<T>(segment.Array, segment.Offset, segment.Count);

    /// <summary>
    /// Returns a 0-length read-only span whose base is the null pointer.
    /// </summary>
    public static ReadOnlySpan<T> Empty => default;

    /// <summary>Gets an enumerator for this span.</summary>
    public Enumerator GetEnumerator() => new Enumerator(this);

    /// <summary>Enumerates the elements of a <see cref="ReadOnlySpan{T}"/>.</summary>
    public ref struct Enumerator
    {
        /// <summary>The span being enumerated.</summary>
        private readonly ReadOnlySpan<T> _span;
        /// <summary>The next index to yield.</summary>
        private int _index;

        /// <summary>Initialize the enumerator.</summary>
        /// <param name="span">The span to enumerate.</param>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        internal Enumerator(ReadOnlySpan<T> span)
        {
            _span = span;
            _index = -1;
        }

        /// <summary>Advances the enumerator to the next element of the span.</summary>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public bool MoveNext()
        {
            int index = _index + 1;
            if (index < _span.Length)
            {
                _index = index;
                return true;
            }

            return false;
        }

        /// <summary>Gets the element at the current position of the enumerator.</summary>
        public ref readonly T Current
        {
            [MethodImpl(MethodImplOptions.AggressiveInlining)]
            get => ref _span[_index];
        }
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private unsafe void DoCopy(Span<T> destination)
    {
        // this branch should be optimized out when the instance of the function is created
        // for the type because this is a jit time constant 
        if (RuntimeHelpers.IsReferenceOrContainsReferences<T>())
        {
            // if this is a managed type then we will need to do a more expensive
            // copy because we need GC barriers, we will make it a tad faster by 
            // removing the length checks
            // TODO: in the future this can be made faster by not doing a GC barrier
            //       every iteration but every X iterations or something alike 
            
            ref var ptr = ref Unsafe.AsRef<T>(_pointer._value);
            ref var dstPtr = ref Unsafe.AsRef<T>(destination._pointer._value);
            
            if (Unsafe.IsAddressLessThan(ref ptr, ref dstPtr) && Unsafe.IsAddressLessThan(ref dstPtr, ref Unsafe.Add<T>(ref ptr, _length)))
            {
                for (var i = _length - 1; i >= 0; i--)
                {
                    Unsafe.Add(ref dstPtr, i) = Unsafe.Add(ref ptr, i);
                }
            }
            else
            {
                for (var i = _length - 1; i >= 0; i--)
                {
                    Unsafe.Add(ref dstPtr, i) = Unsafe.Add(ref ptr, i);
                }
            }
        }
        else
        {
            // For unmanaged types we can use the native memmove function which should be quite fast
            ref var ptr = ref Unsafe.AsRef<byte>(_pointer._value);
            ref var dstPtr = ref Unsafe.AsRef<byte>(destination._pointer._value);
            Buffer.Memmove(ref dstPtr, ref ptr, ((nuint)Unsafe.SizeOf<T>() * (nuint)Length));
        }
    }
    
    /// <summary>
    /// Copies the contents of this read-only span into destination span. If the source
    /// and destinations overlap, this method behaves as if the original values in
    /// a temporary location before the destination is overwritten.
    ///
    /// <param name="destination">The span to copy items into.</param>
    /// <exception cref="System.ArgumentException">
    /// Thrown when the destination Span is shorter than the source Span.
    /// </exception>
    /// </summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public void CopyTo(Span<T> destination)
    {
        // Using "if (!TryCopyTo(...))" results in two branches: one for the length
        // check, and one for the result of TryCopyTo. Since these checks are equivalent,
        // we can optimize by performing the check once ourselves then calling Memmove directly.

        if ((uint)_length > destination.Length)
            ThrowHelper.ThrowArgumentException_DestinationTooShort();

        DoCopy(destination);
    }
    
    /// <summary>
    /// Copies the contents of this read-only span into destination span. If the source
    /// and destinations overlap, this method behaves as if the original values in
    /// a temporary location before the destination is overwritten.
    /// </summary>
    /// <returns>If the destination span is shorter than the source span, this method
    /// return false and no data is written to the destination.</returns>
    /// <param name="destination">The span to copy items into.</param>
    public unsafe bool TryCopyTo(Span<T> destination)
    {
        if ((uint)_length > (uint)destination.Length) 
            return false;
        
        DoCopy(destination);

        return true;
    }
    
    /// <summary>
    /// Returns true if left and right point at the same memory and have the same length.  Note that
    /// this does *not* check to see if the *contents* are equal.
    /// </summary>
    public static bool operator ==(ReadOnlySpan<T> left, ReadOnlySpan<T> right) =>
        left._length == right._length &&
        Unsafe.AreSame<T>(ref left._pointer.Value, ref right._pointer.Value);

    /// <summary>
    /// For <see cref="ReadOnlySpan{Char}"/>, returns a new instance of string that represents the characters pointed to by the span.
    /// Otherwise, returns a <see cref="string"/> with the name of the type and the number of elements.
    /// </summary>
    public override string ToString()
    {
        if (typeof(T) == typeof(char))
        {
            return new string(new ReadOnlySpan<char>(ref Unsafe.As<T, char>(ref _pointer.Value), _length));
        }
        return $"System.ReadOnlySpan<{typeof(T).Name}>[{_length}]";
    }
    
    /// <summary>
    /// Forms a slice out of the given read-only span, beginning at 'start'.
    /// </summary>
    /// <param name="start">The index at which to begin this slice.</param>
    /// <exception cref="System.ArgumentOutOfRangeException">
    /// Thrown when the specified <paramref name="start"/> index is not in range (&lt;0 or &gt;Length).
    /// </exception>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public ReadOnlySpan<T> Slice(int start)
    {
        if ((uint)start > (uint)_length)
            ThrowHelper.ThrowArgumentOutOfRangeException();

        return new ReadOnlySpan<T>(ref Unsafe.Add(ref _pointer.Value, (nint)(uint)start /* force zero-extension */), _length - start);
    }
    
    /// <summary>
    /// Forms a slice out of the given read-only span, beginning at 'start', of given length
    /// </summary>
    /// <param name="start">The index at which to begin this slice.</param>
    /// <param name="length">The desired length for the slice (exclusive).</param>
    /// <exception cref="System.ArgumentOutOfRangeException">
    /// Thrown when the specified <paramref name="start"/> or end index is not in range (&lt;0 or &gt;Length).
    /// </exception>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public ReadOnlySpan<T> Slice(int start, int length)
    {
        // See comment in Span<T>.Slice for how this works.
        if ((ulong)(uint)start + (ulong)(uint)length > (ulong)(uint)_length)
            ThrowHelper.ThrowArgumentOutOfRangeException();

        return new ReadOnlySpan<T>(ref Unsafe.Add(ref _pointer.Value, (nint)(uint)start /* force zero-extension */), length);
    }

    /// <summary>
    /// Copies the contents of this read-only span into a new array.  This heap
    /// allocates, so should generally be avoided, however it is sometimes
    /// necessary to bridge the gap with APIs written in terms of arrays.
    /// </summary>
    public T[] ToArray()
    {
        if (_length == 0)
            return Array.Empty<T>();

        var destination = new T[_length];
        CopyTo(destination);
        return destination;
    }
    
}