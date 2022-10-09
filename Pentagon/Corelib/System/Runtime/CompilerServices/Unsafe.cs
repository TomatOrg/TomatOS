// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

namespace System.Runtime.CompilerServices;

/// <summary>
/// Unlike the standard Corelib Unsafe interface, most of the methods here are actually
/// internal, and may not be used outside of the corelib, this is to ensure memory safety
/// while still allowing the corelib to expose safe interfaces to the userspace.
///
/// Some methods in here are public, and its because they have no risk of reducing the memory
/// safety we seek for.
/// </summary>
public static unsafe class Unsafe
{
    /// <summary>
    /// Returns a pointer to the given by-ref parameter.
    /// </summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining, MethodCodeType = MethodCodeType.Runtime)]
    internal static extern void* AsPointer<T> (ref T value);
        
    /// <summary>
    /// Returns the size of an object of the given type parameter.
    /// </summary>
    [MethodImpl(MethodCodeType = MethodCodeType.Runtime)]
    public static extern int SizeOf<T>();

    /// <summary>
    /// Casts the given object to the specified type, performs no dynamic type checking.
    /// </summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining, MethodCodeType = MethodCodeType.Runtime)]
    internal static extern T As<T>(object o) where T : class;

    /// <summary>
    /// Reinterprets the given reference as a reference to a value of type <typeparamref name="TTo"/>.
    /// </summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining, MethodCodeType = MethodCodeType.Runtime)]
    internal static extern ref TTo As<TFrom, TTo>(ref TFrom source);

    /// <summary>
    /// Adds an element offset to the given reference.
    /// </summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static ref T Add<T>(ref T source, int elementOffset)
    {
        return ref AddByteOffset(ref source, (nint)elementOffset * SizeOf<T>());
    }
    
    /// <summary>
    /// Adds an element offset to the given reference.
    /// </summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static ref T Add<T>(ref T source, nint elementOffset)
    {
        return ref AddByteOffset(ref source, elementOffset * SizeOf<T>());
    }
    
    /// <summary>
    /// Adds an element offset to the given pointer.
    /// </summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static void* Add<T>(void* source, int elementOffset)
    {
        return (byte*)source + (elementOffset * (nint)SizeOf<T>());
    }
    
    /// <summary>
    /// Adds an byte offset to the given reference.
    /// </summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static ref T AddByteOffset<T>(ref T source, nuint byteOffset)
    {
        return ref AddByteOffset(ref source, (nint)byteOffset);
    }
    
    /// <summary>
    /// Determines whether the specified references point to the same location.
    /// </summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static bool AreSame<T>(ref T left, ref T right)
    {
        return AsPointer(ref left) == AsPointer(ref right);
    }
    
    /// <summary>
    /// Determines whether the memory address referenced by <paramref name="left"/> is greater than
    /// the memory address referenced by <paramref name="right"/>.
    /// </summary>
    /// <remarks>
    /// This check is conceptually similar to "(void*)(&amp;left) &gt; (void*)(&amp;right)".
    /// </remarks>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static bool IsAddressGreaterThan<T>(ref T left, ref T right)
    {
        return AsPointer(ref left) > AsPointer(ref right);
    }
    
    /// <summary>
    /// Determines whether the memory address referenced by <paramref name="left"/> is less than
    /// the memory address referenced by <paramref name="right"/>.
    /// </summary>
    /// <remarks>
    /// This check is conceptually similar to "(void*)(&amp;left) &lt; (void*)(&amp;right)".
    /// </remarks>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static bool IsAddressLessThan<T>(ref T left, ref T right)
    {
        return AsPointer(ref left) < AsPointer(ref right);
    }
    
    /// <summary>
    /// Initializes a block of memory at the given location with a given initial value
    /// without assuming architecture dependent alignment of the address.
    /// </summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static void InitBlockUnaligned(ref byte startAddress, byte value, uint byteCount)
    {
        for (uint i = 0; i < byteCount; i++)
            AddByteOffset(ref startAddress, i) = value;
    }
    
    /// <summary>
    /// Reads a value of type <typeparamref name="T"/> from the given location.
    /// </summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static T ReadUnaligned<T>(void* source)
    {
        return AsRef<T>(source);
    }
    
    /// <summary>
    /// Reads a value of type <typeparamref name="T"/> from the given location.
    /// </summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static T ReadUnaligned<T>(ref byte source)
    {
        return As<byte, T>(ref source);
    }
    
    /// <summary>
    /// Writes a value of type <typeparamref name="T"/> to the given location.
    /// </summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static void WriteUnaligned<T>(void* destination, T value)
    {
        AsRef<T>(destination) = value;
    }

    /// <summary>
    /// Writes a value of type <typeparamref name="T"/> to the given location.
    /// </summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static void WriteUnaligned<T>(ref byte destination, T value)
    {
        As<byte, T>(ref destination) = value;
    }
    
    /// <summary>
    /// Adds an byte offset to the given reference.
    /// </summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static ref T AddByteOffset<T>(ref T source, nint byteOffset)
    {
        var ptr = AsPointer(ref source);
        ptr = (void*)((ulong)ptr + (ulong)byteOffset);
        return ref AsRef<T>(ptr);
    }

    /// <summary>
    /// Reads a value of type <typeparamref name="T"/> from the given location.
    /// </summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static T Read<T>(void* source)
    {
        return AsRef<T>(source);
    }
    
    /// <summary>
    /// Reads a value of type <typeparamref name="T"/> from the given location.
    /// </summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static T Read<T>(ref byte source)
    {
        return As<byte, T>(ref source);
    }
    
    /// <summary>
    /// Writes a value of type <typeparamref name="T"/> to the given location.
    /// </summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static void Write<T>(void* destination, T value)
    {
        AsRef<T>(destination) = value;
    }
    
    /// <summary>
    /// Writes a value of type <typeparamref name="T"/> to the given location.
    /// </summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static void Write<T>(ref byte destination, T value)
    {
        As<byte, T>(ref destination) = value;
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining, MethodCodeType = MethodCodeType.Runtime)]
    internal static extern ref T AsRef<T>(void* source);

    /// <summary>
    /// Determines the byte offset from origin to target from the given references.
    /// </summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static nint ByteOffset<T>(ref T origin, ref T target)
    {
        return (nint)AsPointer(ref target) - (nint)AsPointer(ref origin);
    }
    
    /// <summary>
    /// Returns a by-ref to type <typeparamref name="T"/> that is a null reference.
    /// </summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static ref T NullRef<T>()
    {
        return ref AsRef<T>(null);
    }
    
    /// <summary>
    /// Returns if a given by-ref to type <typeparamref name="T"/> is a null reference.
    /// </summary>
    /// <remarks>
    /// This check is conceptually similar to "(void*)(&amp;source) == nullptr".
    /// </remarks>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static bool IsNullRef<T>(ref T source)
    {
        return AsPointer(ref source) == null;
    }
    
}