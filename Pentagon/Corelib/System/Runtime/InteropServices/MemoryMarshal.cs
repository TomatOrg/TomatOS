using System.Runtime.CompilerServices;

namespace System.Runtime.InteropServices;

public static class MemoryMarshal
{

    public static Span<byte> AsBytes<T>(Span<T> span)
        where T : unmanaged
    {
        // TODO: checked
        return new Span<byte>(span._ptr, span.Length * Unsafe.SizeOf<T>());
    }

    public static Span<TTo> Cast<TFrom, TTo>(Span<TFrom> span)
        where TFrom : unmanaged
        where TTo : unmanaged
    {
        var fromSize = (uint)Unsafe.SizeOf<TFrom>();
        var toSize = (uint)Unsafe.SizeOf<TTo>();
        var fromLength = (uint)span.Length;
        // TODO: checked
        var toLength = (int)((ulong)fromLength * (ulong)fromSize / (ulong)toSize);
        return new Span<TTo>(span._ptr, toLength);
    }

    public static Memory<TTo> Cast<TFrom, TTo>(Memory<TFrom> mem)
        where TFrom : unmanaged
        where TTo : unmanaged
    {
        var fromSize = (uint)Unsafe.SizeOf<TFrom>();
        var toSize = (uint)Unsafe.SizeOf<TTo>();
        var fromLength = (uint)mem.Length;
        // TODO: checked
        var toLength = (int)((ulong)fromLength * (ulong)fromSize / (ulong)toSize);
        return new Memory<TTo>(mem._obj, mem._ptr, toLength);
    }

    // TODO: readonly
    
    public static T Read<T>(Span<byte> source)
        where T : unmanaged
    {
        if (Unsafe.SizeOf<T>() > source.Length)
            throw new ArgumentOutOfRangeException(nameof(source.Length));
        return new Span<T>(source._ptr, 1)[0];
    }
    
    public static bool TryRead<T>(Span<byte> source, out T value)
        where T : unmanaged
    {
        if (Unsafe.SizeOf<T>() > source.Length)
        {
            value = default;
            return false;
        }
        else
        {
            value = new Span<T>(source._ptr, 1)[0];
            return true;
        }
    }

    public static void Write<T>(Span<byte> source, in T value)
        where T : unmanaged
    {
        if (Unsafe.SizeOf<T>() > source.Length)
            throw new ArgumentOutOfRangeException(nameof(source.Length));
        
        new Span<T>(source._ptr, 1)[0] = value;
    }
    
    public static bool TryWrite<T>(Span<byte> source, in T value)
        where T : unmanaged
    {
        if (Unsafe.SizeOf<T>() > source.Length)
        {
            return false;
        }
        else
        {
            new Span<T>(source._ptr, 1)[0] = value;
            return true;
        }
    }
}