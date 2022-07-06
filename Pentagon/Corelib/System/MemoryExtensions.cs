namespace System;

public static class MemoryExtensions
{

    public static Memory<T> AsMemory<T>(this T[] array)
    {
        return array;
    }

    public static Memory<T> AsMemory<T>(this T[] array, int start)
    {
        if (array == null && start != 0)
            throw new ArgumentOutOfRangeException();

        return array == null ? Memory<T>.Empty : new Memory<T>(array, start, array.Length - start);
    }
    
    public static Memory<T> AsMemory<T>(this T[] array, int start, int length)
    {
        return new Memory<T>(array, start, length);
    }

    public static Span<T> AsSpan<T>(this T[] array)
    {
        return array;
    }
    
    public static Span<T> AsSpan<T>(this T[] array, int start)
    {
        if (array == null && start != 0)
            throw new ArgumentOutOfRangeException();

        return array == null ? Span<T>.Empty : new Span<T>(array, start, array.Length - start);
    }

    public static Span<T> AsSpan<T>(this T[] array, int start, int length)
    {
        return new Span<T>(array, start, length);
    }
    
}