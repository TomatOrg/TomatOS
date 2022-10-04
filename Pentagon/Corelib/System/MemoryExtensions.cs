// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace System;

public static class MemoryExtensions
{

    #region AsMemory

    /// <summary>
    /// Creates a new memory over the target array.
    /// </summary>
    public static Memory<T> AsMemory<T>(this T[]? array) => new Memory<T>(array);

    /// <summary>
    /// Creates a new memory over the portion of the target array beginning
    /// at 'start' index and ending at 'end' index (exclusive).
    /// </summary>
    /// <param name="array">The target array.</param>
    /// <param name="start">The index at which to begin the memory.</param>
    /// <remarks>Returns default when <paramref name="array"/> is null.</remarks>
    /// <exception cref="System.ArrayTypeMismatchException">Thrown when <paramref name="array"/> is covariant and array's type is not exactly T[].</exception>
    /// <exception cref="System.ArgumentOutOfRangeException">
    /// Thrown when the specified <paramref name="start"/> or end index is not in the range (&lt;0 or &gt;array.Length).
    /// </exception>
    public static Memory<T> AsMemory<T>(this T[]? array, int start) => new Memory<T>(array, start);

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
    public static Memory<T> AsMemory<T>(this T[]? array, int start, int length) => new Memory<T>(array, start, length);

    /// <summary>Creates a new <see cref="ReadOnlyMemory{T}"/> over the portion of the target string.</summary>
    /// <param name="text">The target string.</param>
    /// <remarks>Returns default when <paramref name="text"/> is null.</remarks>
    public static ReadOnlyMemory<char> AsMemory(this string? text)
    {
        if (text == null)
            return default;

        return new ReadOnlyMemory<char>(text, ref text.GetRawStringData(), text.Length);
    }

    /// <summary>Creates a new <see cref="ReadOnlyMemory{T}"/> over the portion of the target string.</summary>
    /// <param name="text">The target string.</param>
    /// <param name="start">The index at which to begin this slice.</param>
    /// <remarks>Returns default when <paramref name="text"/> is null.</remarks>
    /// <exception cref="System.ArgumentOutOfRangeException">
    /// Thrown when the specified <paramref name="start"/> index is not in range (&lt;0 or &gt;text.Length).
    /// </exception>
    public static ReadOnlyMemory<char> AsMemory(this string? text, int start)
    {
        if (text == null)
        {
            if (start != 0)
                ThrowHelper.ThrowArgumentOutOfRangeException(ExceptionArgument.start);
            return default;
        }

        if ((uint)start > (uint)text.Length)
            ThrowHelper.ThrowArgumentOutOfRangeException(ExceptionArgument.start);

        return new ReadOnlyMemory<char>(text, ref Unsafe.Add(ref text.GetRawStringData(), (nint)(uint)start /* force zero-extension */), text.Length - start);
    }
    
    
    /// <summary>Creates a new <see cref="ReadOnlyMemory{T}"/> over the portion of the target string.</summary>
    /// <param name="text">The target string.</param>
    /// <param name="start">The index at which to begin this slice.</param>
    /// <param name="length">The desired length for the slice (exclusive).</param>
    /// <remarks>Returns default when <paramref name="text"/> is null.</remarks>
    /// <exception cref="System.ArgumentOutOfRangeException">
    /// Thrown when the specified <paramref name="start"/> index or <paramref name="length"/> is not in range.
    /// </exception>
    public static ReadOnlyMemory<char> AsMemory(this string? text, int start, int length)
    {
        if (text == null)
        {
            if (start != 0 || length != 0)
                ThrowHelper.ThrowArgumentOutOfRangeException(ExceptionArgument.start);
            return default;
        }

        // See comment in Span<T>.Slice for how this works.
        if ((ulong)(uint)start + (ulong)(uint)length > (ulong)(uint)text.Length)
            ThrowHelper.ThrowArgumentOutOfRangeException(ExceptionArgument.start);

        return new ReadOnlyMemory<char>(text, ref Unsafe.Add(ref text.GetRawStringData(), (nint)(uint)start /* force zero-extension */), length);
    }
    
    #endregion

    #region AsSpan
    
    /// <summary>
    /// Creates a new span over the target array.
    /// </summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Span<T> AsSpan<T>(this T[]? array)
    {
        return new Span<T>(array);
    }

    /// <summary>
    /// Creates a new span over the portion of the target array.
    /// </summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Span<T> AsSpan<T>(this T[]? array, int start)
    {
        if (array == null)
        {
            if (start != 0)
                ThrowHelper.ThrowArgumentOutOfRangeException();
            return default;
        }
        if ((uint)start > (uint)array.Length)
            ThrowHelper.ThrowArgumentOutOfRangeException();

        return new Span<T>(ref Unsafe.Add(ref MemoryMarshal.GetArrayDataReference(array), (nint)(uint)start /* force zero-extension */), array.Length - start);
    }

    /// <summary>
    /// Creates a new Span over the portion of the target array beginning
    /// at 'start' index and ending at 'end' index (exclusive).
    /// </summary>
    /// <param name="array">The target array.</param>
    /// <param name="start">The index at which to begin the Span.</param>
    /// <param name="length">The number of items in the Span.</param>
    /// <remarks>Returns default when <paramref name="array"/> is null.</remarks>
    /// <exception cref="System.ArrayTypeMismatchException">Thrown when <paramref name="array"/> is covariant and array's type is not exactly T[].</exception>
    /// <exception cref="System.ArgumentOutOfRangeException">
    /// Thrown when the specified <paramref name="start"/> or end index is not in the range (&lt;0 or &gt;Length).
    /// </exception>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Span<T> AsSpan<T>(this T[]? array, int start, int length)
    {
        return new Span<T>(array, start, length);
    }

    /// <summary>
    /// Creates a new readonly span over the portion of the target string.
    /// </summary>
    /// <param name="text">The target string.</param>
    /// <remarks>Returns default when <paramref name="text"/> is null.</remarks>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static ReadOnlySpan<char> AsSpan(this string? text)
    {
        if (text == null)
            return default;

        return new ReadOnlySpan<char>(ref text.GetRawStringData(), text.Length);
    }
    
    /// <summary>
    /// Creates a new readonly span over the portion of the target string.
    /// </summary>
    /// <param name="text">The target string.</param>
    /// <param name="start">The index at which to begin this slice.</param>
    /// <exception cref="System.ArgumentNullException">Thrown when <paramref name="text"/> is null.</exception>
    /// <exception cref="System.ArgumentOutOfRangeException">
    /// Thrown when the specified <paramref name="start"/> index is not in range (&lt;0 or &gt;text.Length).
    /// </exception>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static ReadOnlySpan<char> AsSpan(this string? text, int start)
    {
        if (text == null)
        {
            if (start != 0)
                ThrowHelper.ThrowArgumentOutOfRangeException(ExceptionArgument.start);
            return default;
        }

        if ((uint)start > (uint)text.Length)
            ThrowHelper.ThrowArgumentOutOfRangeException(ExceptionArgument.start);

        return new ReadOnlySpan<char>(ref Unsafe.Add(ref text.GetRawStringData(), (nint)(uint)start /* force zero-extension */), text.Length - start);
    }
    
    /// <summary>
    /// Creates a new readonly span over the portion of the target string.
    /// </summary>
    /// <param name="text">The target string.</param>
    /// <param name="start">The index at which to begin this slice.</param>
    /// <param name="length">The desired length for the slice (exclusive).</param>
    /// <remarks>Returns default when <paramref name="text"/> is null.</remarks>
    /// <exception cref="System.ArgumentOutOfRangeException">
    /// Thrown when the specified <paramref name="start"/> index or <paramref name="length"/> is not in range.
    /// </exception>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static ReadOnlySpan<char> AsSpan(this string? text, int start, int length)
    {
        if (text == null)
        {
            if (start != 0 || length != 0)
                ThrowHelper.ThrowArgumentOutOfRangeException(ExceptionArgument.start);
            return default;
        }

        // See comment in Span<T>.Slice for how this works.
        if ((ulong)(uint)start + (ulong)(uint)length > (ulong)(uint)text.Length)
            ThrowHelper.ThrowArgumentOutOfRangeException(ExceptionArgument.start);

        return new ReadOnlySpan<char>(ref Unsafe.Add(ref text.GetRawStringData(), (nint)(uint)start /* force zero-extension */), length);
    }
    
    #endregion

    #region CopyTo

    /// <summary>
    /// Copies the contents of the array into the span. If the source
    /// and destinations overlap, this method behaves as if the original values in
    /// a temporary location before the destination is overwritten.
    ///
    ///<param name="source">The array to copy items from.</param>
    /// <param name="destination">The span to copy items into.</param>
    /// <exception cref="System.ArgumentException">
    /// Thrown when the destination Span is shorter than the source array.
    /// </exception>
    /// </summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static void CopyTo<T>(this T[]? source, Span<T> destination)
    {
        new ReadOnlySpan<T>(source).CopyTo(destination);
    }

    /// <summary>
    /// Copies the contents of the array into the memory. If the source
    /// and destinations overlap, this method behaves as if the original values are in
    /// a temporary location before the destination is overwritten.
    ///
    ///<param name="source">The array to copy items from.</param>
    /// <param name="destination">The memory to copy items into.</param>
    /// <exception cref="System.ArgumentException">
    /// Thrown when the destination is shorter than the source array.
    /// </exception>
    /// </summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static void CopyTo<T>(this T[]? source, Memory<T> destination)
    {
        source.CopyTo(destination.Span);
    }

    #endregion

    #region SequenceEqual

    
    /// <summary>
    /// Determines whether two sequences are equal by comparing the elements using IEquatable{T}.Equals(T).
    /// </summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static bool SequenceEqual<T>(this Span<T> span, ReadOnlySpan<T> other) where T : IEquatable<T>
    {
        int length = span.Length;

        if (RuntimeHelpers.IsBitwiseEquatable<T>())
        {
            nuint size = (nuint)Unsafe.SizeOf<T>();
            return length == other.Length &&
                   SpanHelpers.SequenceEqual(
                       ref Unsafe.As<T, byte>(ref MemoryMarshal.GetReference(span)),
                       ref Unsafe.As<T, byte>(ref MemoryMarshal.GetReference(other)),
                       ((uint)length) * size);  // If this multiplication overflows, the Span we got overflows the entire address range. There's no happy outcome for this api in such a case so we choose not to take the overhead of checking.
        }

        return length == other.Length && SpanHelpers.SequenceEqual(ref MemoryMarshal.GetReference(span), ref MemoryMarshal.GetReference(other), length);
    }

    #endregion
    
}