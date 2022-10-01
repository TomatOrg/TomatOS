using System.Collections;
using System.Collections.Generic;
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

    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    internal extern unsafe void* GetDataPtr();

    #endregion
    
    private static class EmptyArray<T>
    {
        internal static readonly T[] Value = new T[0];
    }

    public static T[] Empty<T>()
    {
        return EmptyArray<T>.Value;
    }
    
    public object Clone()
    {
        return MemberwiseClone();
    }

    #region Clear

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

    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
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
    
    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    private static extern void CopyInternal(Array sourceArray, long sourceIndex, Array destinationArray, long destinationIndex, long length);

    #endregion

    #region Fill

    public static void Fill<T>(T[] array, T value)
    {
        array.AsSpan().Fill(value);
        Fill(array, value, 0, array.Length);
    }
    
    public static void Fill<T>(T[] array, T value, int startIndex, int count)
    {
        array.AsSpan(startIndex, count).Fill(value);
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

    private class GenericArray<T> : Array, ICollection<T>, IList<T>, IEnumerable<T>
    {
        
        private class GenericEnumerator : IEnumerator<T>
        {

            private T[] _array;
            private int _index;

            public GenericEnumerator(T[] array)
            {
                _array = array;
                _index = -1;
            }

            public T Current
            {
                get
                {
                    if (_index < 0)
                        throw new InvalidOperationException("Enumeration has not started");
                    if (_index >= _array._length)
                        throw new InvalidOperationException("Enumeration has finished");
                    return _array[_index];
                }
            }

            object IEnumerator.Current => Current;

            public bool MoveNext()
            {
                _index++;
                return (_index < _array._length);
            }

            public void Reset()
            {
                _index = -1;
            }
        
            public void Dispose()
            {
            }

        }

        public IEnumerator<T> GetEnumerator()
        {
            return new GenericEnumerator(Unsafe.As<T[]>(this));
        }

        IEnumerator IEnumerable.GetEnumerator()
        {
            return GetEnumerator();
        }

        public int Count => _length;
        public bool IsReadOnly => false;
        public void Add(T item)
        {
            throw new NotSupportedException();
        }

        public void Clear()
        {
            Array.Clear(this);
        }

        public bool Contains(T item)
        {
            return Array.IndexOf(Unsafe.As<T[]>(this), item) != -1;
        }

        public void CopyTo(T[] array, int arrayIndex)
        {
            Array.Copy(this, 0, array, arrayIndex, array.Length);
        }

        public bool Remove(T item)
        {
            throw new NotSupportedException();
        }

        public T this[int index]
        {
            get => Unsafe.As<T[]>(this)[index];
            set => Unsafe.As<T[]>(this)[index] = value;
        }

        public int IndexOf(T item)
        {
            return Array.IndexOf(Unsafe.As<T[]>(this), item);
        }

        public void Insert(int index, T item)
        {
            throw new NotSupportedException();
        }

        public void RemoveAt(int index)
        {
            throw new NotSupportedException();
        }
    }
    

}