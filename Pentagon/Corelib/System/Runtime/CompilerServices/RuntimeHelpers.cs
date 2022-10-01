using System.Runtime.Serialization;

namespace System.Runtime.CompilerServices;

public static class RuntimeHelpers
{
    
    [MethodImpl(MethodImplOptions.AggressiveInlining, MethodCodeType = MethodCodeType.Runtime)]
    public static extern bool IsReferenceOrContainsReferences<T>();

    // TODO: what should the logic for this be?
    public static bool IsBitwiseEquatable<T>()
    {
        return typeof(T) == typeof(bool) ||
               typeof(T) == typeof(char) ||
               typeof(T) == typeof(sbyte) ||
               typeof(T) == typeof(byte) ||
               typeof(T) == typeof(short) ||
               typeof(T) == typeof(ushort) ||
               typeof(T) == typeof(int) ||
               typeof(T) == typeof(uint) ||
               typeof(T) == typeof(long) ||
               typeof(T) == typeof(ulong) ||
               typeof(T) == typeof(float) ||
               typeof(T) == typeof(double);
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining, MethodCodeType = MethodCodeType.Runtime)]
    private static extern unsafe void* GetObjectPointer(object obj);
    
    public static unsafe int GetHashCode(object? o)
    {
        return HashCode.Combine((nint)GetObjectPointer(o));
    }
    
}