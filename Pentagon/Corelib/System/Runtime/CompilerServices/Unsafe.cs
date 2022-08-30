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
    
    // This is public because I consider it safe enough, since the user
    // has no way to actually access pointer values
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static void* Add<T>(void* source, int elementOffset)
    {
        return (void*)((ulong)source + (ulong)elementOffset * (ulong)SizeOf<T>());
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static ref T Add<T>(ref T source, int elementOffset)
    {
        return ref AsRef<T>(Add<T>(AsPointer(ref source), elementOffset));
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static ref T AddByteOffset<T>(ref T source, UIntPtr byteOffset)
    {
        var ptr = AsPointer(ref source);
        ptr = (void*)((ulong)ptr + byteOffset.ToUInt64());
        return ref AsRef<T>(ptr);
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static bool AreSame<T>(ref T left, ref T right)
    {
        return AsPointer(ref left) == AsPointer(ref right);
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining, MethodCodeType = MethodCodeType.Runtime)]
    internal static extern void* AsPointer<T> (ref T value);
    
    [MethodImpl(MethodImplOptions.AggressiveInlining, MethodCodeType = MethodCodeType.Runtime)]
    internal static extern ref T AsRef<T>(void* source);
    
    [MethodImpl(MethodImplOptions.AggressiveInlining, MethodCodeType = MethodCodeType.Runtime)]
    internal static extern T As<T>(object o) where T : class;

    [MethodImpl(MethodImplOptions.AggressiveInlining, MethodCodeType = MethodCodeType.Runtime)]
    internal static extern ref TTo As<TFrom, TTo>(ref TFrom source);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static void Copy<T>(void* destination, ref T source)
    {
        AsRef<T>(destination) = source;
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static void Copy<T>(ref T destination, void* source)
    {
        destination = AsRef<T>(source);
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static T Read<T>(void* source)
    {
        return AsRef<T>(source);
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static T ReadUnaligned<T>(void* source)
    {
        return AsRef<T>(source);
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static T ReadUnaligned<T>(ref byte source)
    {
        return AsRef<T>(AsPointer(ref source));
    }
    
    // can be public, its just a size
    [MethodImpl(MethodCodeType = MethodCodeType.Runtime)]
    public static extern int SizeOf<T>();

    
    // This is public because I consider it safe enough, since the user
    // has no way to actually access pointer values
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static void* Subtract<T>(void* source, int elementOffset)
    {
        return (void*)((ulong)source - (ulong)elementOffset * (ulong)SizeOf<T>());
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static ref T Subtract<T>(ref T source, int elementOffset)
    {
        return ref AsRef<T>(Subtract<T>(AsPointer(ref source), elementOffset));
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static ref T SubtractByteOffset<T>(ref T source, UIntPtr byteOffset)
    {
        var ptr = AsPointer(ref source);
        ptr = (void*)((ulong)ptr - byteOffset.ToUInt64());
        return ref AsRef<T>(ptr);
    }
    
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static void Write<T>(void* destination, T value)
    {
        AsRef<T>(destination) = value;
    }
    
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static void WriteUnaligned<T>(ref byte destination, T value)
    {
        AsRef<T>(AsPointer(ref destination)) = value;
    }
    
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static void WriteUnaligned<T>(void* destination, T value)
    {
        AsRef<T>(destination) = value;
    }
}