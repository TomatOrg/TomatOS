// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System.Collections.Generic;
using System.Runtime.CompilerServices;

namespace System.Runtime.InteropServices;

/// <summary>
/// Provides a collection of methods for interoperating with <see cref="Memory{T}"/>, <see cref="ReadOnlyMemory{T}"/>,
/// <see cref="Span{T}"/>, and <see cref="ReadOnlySpan{T}"/>.
/// </summary>
public static class MemoryMarshal
{

    /// <summary>
    /// Casts a Span of one primitive type <typeparamref name="T"/> to Span of bytes.
    /// That type may not contain pointers or references.
    /// </summary>
    /// <param name="span">The source slice, of type <typeparamref name="T"/>.</param>
    /// <exception cref="System.ArgumentException">
    /// Thrown when <typeparamref name="T"/> contains pointers.
    /// </exception>
    /// <exception cref="System.OverflowException">
    /// Thrown if the Length property of the new Span would exceed int.MaxValue.
    /// </exception>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Span<byte> AsBytes<T>(Span<T> span)
        where T : unmanaged
    {
        return new Span<byte>(
            ref Unsafe.As<T, byte>(ref GetReference(span)),
            /*checked*/(span.Length * Unsafe.SizeOf<T>()));
    }
    
    /// <summary>
    /// Casts a ReadOnlySpan of one primitive type <typeparamref name="T"/> to ReadOnlySpan of bytes.
    /// That type may not contain pointers or references.
    /// </summary>
    /// <param name="span">The source slice, of type <typeparamref name="T"/>.</param>
    /// <exception cref="System.ArgumentException">
    /// Thrown when <typeparamref name="T"/> contains pointers.
    /// </exception>
    /// <exception cref="System.OverflowException">
    /// Thrown if the Length property of the new Span would exceed int.MaxValue.
    /// </exception>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static ReadOnlySpan<byte> AsBytes<T>(ReadOnlySpan<T> span)
        where T : unmanaged
    {
        return new ReadOnlySpan<byte>(
            ref Unsafe.As<T, byte>(ref GetReference(span)),
            checked(span.Length * Unsafe.SizeOf<T>()));
    }
    
    // /// <summary>Creates a <see cref="Memory{T}"/> from a <see cref="ReadOnlyMemory{T}"/>.</summary>
    // /// <param name="memory">The <see cref="ReadOnlyMemory{T}"/>.</param>
    // /// <returns>A <see cref="Memory{T}"/> representing the same memory as the <see cref="ReadOnlyMemory{T}"/>, but writable.</returns>
    // /// <remarks>
    // /// <see cref="AsMemory{T}(ReadOnlyMemory{T})"/> must be used with extreme caution.  <see cref="ReadOnlyMemory{T}"/> is used
    // /// to represent immutable data and other memory that is not meant to be written to; <see cref="Memory{T}"/> instances created
    // /// by <see cref="AsMemory{T}(ReadOnlyMemory{T})"/> should not be written to.  The method exists to enable variables typed
    // /// as <see cref="Memory{T}"/> but only used for reading to store a <see cref="ReadOnlyMemory{T}"/>.
    // /// </remarks>
    // public static Memory<T> AsMemory<T>(ReadOnlyMemory<T> memory) =>
    //     Unsafe.As<ReadOnlyMemory<T>, Memory<T>>(ref memory);

    /// <summary>
    /// Returns a reference to the 0th element of the Span. If the Span is empty, returns a reference to the location where the 0th element
    /// would have been stored. Such a reference may or may not be null. It can be used for pinning but must never be dereferenced.
    /// </summary>
    internal static ref T GetReference<T>(Span<T> span) => ref span._pointer.Value;

    /// <summary>
    /// Returns a reference to the 0th element of the ReadOnlySpan. If the ReadOnlySpan is empty, returns a reference to the location where the 0th element
    /// would have been stored. Such a reference may or may not be null. It can be used for pinning but must never be dereferenced.
    /// </summary>
    internal static ref T GetReference<T>(ReadOnlySpan<T> span) => ref span._pointer.Value;
    
    public static unsafe Span<TTo> Cast<TFrom, TTo>(Span<TFrom> span)
        where TFrom : unmanaged
        where TTo : unmanaged
    {
        var fromSize = (uint)Unsafe.SizeOf<TFrom>();
        var toSize = (uint)Unsafe.SizeOf<TTo>();
        var fromLength = (uint)span.Length;
        // TODO: checked
        var toLength = (int)((ulong)fromLength * (ulong)fromSize / (ulong)toSize);
        return new Span<TTo>(span._pointer._value, toLength);
    }

    public static unsafe ReadOnlySpan<TTo> Cast<TFrom, TTo>(ReadOnlySpan<TFrom> span)
        where TFrom : unmanaged
        where TTo : unmanaged
    {
        var fromSize = (uint)Unsafe.SizeOf<TFrom>();
        var toSize = (uint)Unsafe.SizeOf<TTo>();
        var fromLength = (uint)span.Length;
        // TODO: checked
        var toLength = (int)((ulong)fromLength * (ulong)fromSize / (ulong)toSize);
        return new ReadOnlySpan<TTo>(span._pointer._value, toLength);
    }

    public static unsafe Memory<TTo> Cast<TFrom, TTo>(Memory<TFrom> mem)
        where TFrom : unmanaged
        where TTo : unmanaged
    {
        var fromSize = (uint)Unsafe.SizeOf<TFrom>();
        var toSize = (uint)Unsafe.SizeOf<TTo>();
        var fromLength = (uint)mem.Length;
        // TODO: checked
        var toLength = (int)((ulong)fromLength * (ulong)fromSize / (ulong)toSize);
        return new Memory<TTo>(mem._object, mem._ptr, toLength);
    }
    
    public static unsafe ReadOnlyMemory<TTo> Cast<TFrom, TTo>(ReadOnlyMemory<TFrom> mem)
        where TFrom : unmanaged
        where TTo : unmanaged
    {
        var fromSize = (uint)Unsafe.SizeOf<TFrom>();
        var toSize = (uint)Unsafe.SizeOf<TTo>();
        var fromLength = (uint)mem.Length;
        // TODO: checked
        var toLength = (int)((ulong)fromLength * (ulong)fromSize / (ulong)toSize);
        return new ReadOnlyMemory<TTo>(mem._object, mem._pointer, toLength);
    }
    
    /// <summary>
    /// Creates a new span over a portion of a regular managed object. This can be useful
    /// if part of a managed object represents a "fixed array." This is dangerous because the
    /// <paramref name="length"/> is not checked.
    /// </summary>
    /// <param name="reference">A reference to data.</param>
    /// <param name="length">The number of <typeparamref name="T"/> elements the memory contains.</param>
    /// <returns>A span representing the specified reference and length.</returns>
    /// <remarks>The lifetime of the returned span will not be validated for safety by span-aware languages.</remarks>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static Span<T> CreateSpan<T>(ref T reference, int length) => new Span<T>(ref reference, length);

    /// <summary>
    /// Creates a new read-only span over a portion of a regular managed object. This can be useful
    /// if part of a managed object represents a "fixed array." This is dangerous because the
    /// <paramref name="length"/> is not checked.
    /// </summary>
    /// <param name="reference">A reference to data.</param>
    /// <param name="length">The number of <typeparamref name="T"/> elements the memory contains.</param>
    /// <returns>A read-only span representing the specified reference and length.</returns>
    /// <remarks>The lifetime of the returned span will not be validated for safety by span-aware languages.</remarks>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static ReadOnlySpan<T> CreateReadOnlySpan<T>(ref T reference, int length) => new ReadOnlySpan<T>(ref reference, length);
    
    // /// <summary>
    // /// Creates an <see cref="IEnumerable{T}"/> view of the given <paramref name="memory" /> to allow
    // /// the <paramref name="memory" /> to be used in existing APIs that take an <see cref="IEnumerable{T}"/>.
    // /// </summary>
    // /// <typeparam name="T">The element type of the <paramref name="memory" />.</typeparam>
    // /// <param name="memory">The ReadOnlyMemory to view as an <see cref="IEnumerable{T}"/></param>
    // /// <returns>An <see cref="IEnumerable{T}"/> view of the given <paramref name="memory" /></returns>
    // public static IEnumerable<T> ToEnumerable<T>(ReadOnlyMemory<T> memory)
    // {
    //     for (int i = 0; i < memory.Length; i++)
    //         yield return memory.Span[i];
    // }
    
    /// <summary>
    /// Reads a structure of type T out of a read-only span of bytes.
    /// </summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static T Read<T>(ReadOnlySpan<byte> source)
        where T : unmanaged
    {
        if (Unsafe.SizeOf<T>() > source.Length)
        {
            ThrowHelper.ThrowArgumentOutOfRangeException(ExceptionArgument.length);
        }
        return Unsafe.ReadUnaligned<T>(ref GetReference(source));
    }
    
    /// <summary>
    /// Reads a structure of type T out of a span of bytes.
    /// <returns>If the span is too small to contain the type T, return false.</returns>
    /// </summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static bool TryRead<T>(ReadOnlySpan<byte> source, out T value)
        where T : unmanaged
    {
        if (Unsafe.SizeOf<T>() > (uint)source.Length)
        {
            value = default;
            return false;
        }
        value = Unsafe.ReadUnaligned<T>(ref GetReference(source));
        return true;
    }
    
    /// <summary>
    /// Writes a structure of type T into a span of bytes.
    /// </summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static void Write<T>(Span<byte> destination, ref T value)
        where T : unmanaged
    {
        if ((uint)Unsafe.SizeOf<T>() > (uint)destination.Length)
        {
            ThrowHelper.ThrowArgumentOutOfRangeException(ExceptionArgument.length);
        }
        Unsafe.WriteUnaligned<T>(ref GetReference(destination), value);
    }
    
    /// <summary>
    /// Writes a structure of type T into a span of bytes.
    /// <returns>If the span is too small to contain the type T, return false.</returns>
    /// </summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static bool TryWrite<T>(Span<byte> destination, ref T value)
        where T : unmanaged
    {
        if (Unsafe.SizeOf<T>() > (uint)destination.Length)
        {
            return false;
        }
        Unsafe.WriteUnaligned<T>(ref GetReference(destination), value);
        return true;
    }
    
    /// <summary>
    /// Re-interprets a span of bytes as a reference to structure of type T.
    /// The type may not contain pointers or references.
    /// </summary>
    /// <remarks>
    /// Supported only for platforms that support misaligned memory access or when the memory block is aligned by other means.
    /// </remarks>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static ref T AsRef<T>(Span<byte> span)
        where T : unmanaged
    {
        if (Unsafe.SizeOf<T>() > (uint)span.Length)
        {
            ThrowHelper.ThrowArgumentOutOfRangeException(ExceptionArgument.length);
        }
        return ref Unsafe.As<byte, T>(ref GetReference(span));
    }

    /// <summary>
    /// Re-interprets a span of bytes as a reference to structure of type T.
    /// The type may not contain pointers or references.
    /// </summary>
    /// <remarks>
    /// Supported only for platforms that support misaligned memory access or when the memory block is aligned by other means.
    /// </remarks>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static ref readonly T AsRef<T>(ReadOnlySpan<byte> span)
        where T : unmanaged
    {
        if (Unsafe.SizeOf<T>() > (uint)span.Length)
        {
            ThrowHelper.ThrowArgumentOutOfRangeException(ExceptionArgument.length);
        }
        return ref Unsafe.As<byte, T>(ref GetReference(span));
    }
    
    /// <summary>
    /// Returns a reference to the 0th element of <paramref name="array"/>. If the array is empty, returns a reference to where the 0th element
    /// would have been stored. Such a reference may be used for pinning but must never be dereferenced.
    /// </summary>
    /// <exception cref="NullReferenceException"><paramref name="array"/> is <see langword="null"/>.</exception>
    /// <remarks>
    /// This method does not perform array variance checks. The caller must manually perform any array variance checks
    /// if the caller wishes to write to the returned reference.
    /// </remarks>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static unsafe ref T GetArrayDataReference<T>(T[] array)
    {
        return ref Unsafe.AsRef<T>(array.GetDataPtr());
    }

}