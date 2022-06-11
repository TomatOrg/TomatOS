namespace System.Collections.Generic;

public class List<T> : IList<T>
{
    private int _size;
    private T[] _elements;
    
    public int Count => _size;
    public bool IsReadOnly => false;
    
    public int Capacity
    {
        get => _elements.Length;
        set
        {
            if (_elements.Length < value)
            {
                return;
            }
            Array.Resize(ref _elements, value);
        }
    }

    public List()
        : this(4)
    {
    }

    public List(int capacity)
    {
        _elements = new T[capacity];
    }
    
    public T this[int index]
    {
        get
        {
            if ((uint)index >= _size) throw new ArgumentOutOfRangeException(nameof(index));
            return _elements[index];
        }
        set
        {
            if ((uint)index >= _size) throw new ArgumentOutOfRangeException(nameof(index));
            _elements[index] = value;
        }
    }

    private void EnsureCapacity(int capacity)
    {
        if (capacity < Capacity)
        {
            return;
        }
        Capacity = capacity * 2;
    }
    
    public void Add(T item)
    {
        EnsureCapacity(_size + 1);
        _elements[_size++] = item;
    }

    public void Clear()
    {
        Array.Clear(_elements);
        _size = 0;
    }

    public bool Contains(T item)
    {
        return IndexOf(item) >= 0;
    }

    public void CopyTo(T[] array, int arrayIndex)
    {
        Array.Copy(_elements, 0, array, arrayIndex, _elements.Length);
    }

    public bool Remove(T item)
    {
        throw new NotImplementedException();
    }

    public int IndexOf(T item)
    {
        return Array.IndexOf(_elements, item, 0, _size);
    }

    public void Insert(int index, T item)
    {
        throw new NotImplementedException();
    }

    public void RemoveAt(int index)
    {
        throw new NotImplementedException();
    }

    public IEnumerator<T> GetEnumerator()
    {
        throw new NotImplementedException();
    }

}