using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace System;

[StructLayout(LayoutKind.Sequential)]
public class Array
{
    
    public static int MaxLength => int.MaxValue;

    #region Instance
    
    private readonly int _length;

    public int Length => _length;
    public long LongLength => _length;
    public int Rank => 1;

    private Array() {}

    [MethodImpl(MethodImplOptions.InternalCall)]
    internal extern ulong GetDataPtr();

    #endregion

    #region Copy

    public static void Clear(Array array)
    {
        if (array == null) throw new ArgumentNullException(nameof(array));
        ClearInternal(array, 0, array.Length);
    }

    public static void Clear(Array array, int index, int length)
    {
        if (array == null) throw new ArgumentNullException(nameof(array));
        if (index >= array.Length) throw new ArgumentOutOfRangeException(nameof(index));
        if (length < 0) throw new ArgumentOutOfRangeException(nameof(length));
        if (index + length > array.Length) throw new ArgumentOutOfRangeException(nameof(index));
        ClearInternal(array, index, length);
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern void ClearInternal(Array array, int index, int length);

    #endregion

    #region Copy

    public static void Copy(Array sourceArray, Array destinationArray, int length)
    {
        Copy(sourceArray, destinationArray, (long)length);
    }
    
    public static void Copy(Array sourceArray, Array destinationArray, long length)
    {
        Copy(sourceArray, 0, destinationArray, 0, length);
    }

    public static void Copy(Array sourceArray, int sourceIndex, Array destinationArray, int destinationIndex, int length)
    {
        Copy(sourceArray, (long)sourceIndex, destinationArray, destinationIndex, length);
    }

    public static void Copy(Array sourceArray, long sourceIndex, Array destinationArray, long destinationIndex, long length)
    {
        if (sourceArray == null) throw new ArgumentNullException(nameof(sourceArray));
        if (destinationArray == null) throw new ArgumentNullException(nameof(destinationArray));
        if (sourceIndex < 0 || sourceIndex >= sourceArray.Length) throw new ArgumentOutOfRangeException(nameof(sourceIndex));
        if (destinationIndex < 0 || destinationIndex >= destinationArray.Length) throw new ArgumentOutOfRangeException(nameof(destinationIndex));
        if (length < 0 || length >= Int64.MaxValue) throw new ArgumentOutOfRangeException(nameof(length));
        if (sourceIndex + length > sourceArray.Length) throw new ArgumentOutOfRangeException(nameof(length));
        if (destinationIndex + length > destinationArray.Length) throw new ArgumentOutOfRangeException(nameof(length));
        CopyInternal(sourceArray, sourceIndex, destinationArray, destinationIndex, length);
    }
    
    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern void CopyInternal(Array sourceArray, long sourceIndex, Array destinationArray, long destinationIndex, long length);

    #endregion
    
    private static class EmptyArray<T>
    {
        internal static readonly T[] Value = new T[0];
    }

    public static T[] Empty<T>()
    {
        return EmptyArray<T>.Value;
    }

    #region Fill

    public static void Fill<T>(T[] array, T value)
    {
        Fill(array, value, 0, array.Length);
    }
    
    public static void Fill<T>(T[] array, T value, int startIndex, int count)
    {
        for (var i = startIndex; i < startIndex + count; i++)
        {
            array[i] = value;
        }
    }

    #endregion
    
    #region IndexOF

    public static int IndexOf<T>(T[] array, T value, int startIndex)
    {
        if (array == null) throw new ArgumentNullException(nameof(array));
        return IndexOf(array, value, startIndex, array.Length - startIndex);
    }

    
    public static int IndexOf<T>(T[] array, T value, int startIndex, int count)
    {
        if (array == null) throw new ArgumentNullException(nameof(array));
        if (startIndex >= array.Length) throw new ArgumentOutOfRangeException(nameof(startIndex));
        if (count < 0) throw new ArgumentOutOfRangeException(nameof(count));
        if (startIndex + count > array.Length) throw new ArgumentOutOfRangeException(nameof(startIndex));
        
        for (var i = startIndex; i < startIndex + count; i++)
        {
            // TODO: this does boxing, which I don't like 
            if (array[i].Equals(value))
            {
                return i;
            }
        }
        return -1;
    }

    public static int IndexOf<T>(T[] array, T value)
    {
        if (array == null) throw new ArgumentNullException(nameof(array));
        return IndexOf(array, value, 0, array.Length);
    }

    #endregion
    
    #region Resize

    public static void Resize<T>(ref T[] array, int newSize)
    {
        if (array == null)
        {
            array = new T[newSize];
        }
        else if (array.Length < newSize)
        {
            var arr = new T[newSize];
            Copy(array, arr, array.Length);
            array = arr;
        }
    }

    #endregion
    
}