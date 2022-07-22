using System.Runtime.CompilerServices;

namespace System;

public static class MemoryExtensions
{

    #region AsMemory

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

    #endregion

    #region AsSpan

    // TODO: readonly span...
    internal static Span<char> AsSpan(this string text)
    {
        return text == null ? Span<char>.Empty : new Span<char>(text.GetDataPtr(), text.Length);
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

    #endregion
    
}