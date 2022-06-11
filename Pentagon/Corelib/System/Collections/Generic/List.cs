namespace System.Collections.Generic;

public class List<T>
{

    private T[] _items;
    private int _length = 0;

    public int Capacity
    {
        get => _items.Length;
        set => EnsureCapacity(value);
    }

    public int Count => _length;

    // TODO: ctors

    public List()
    {
        _items = new T[4];
    }

    public T this[int index]
    {
        get
        {
            if ((uint)index >= (uint)_length) throw new ArgumentOutOfRangeException(nameof(index));
            return _items[index];
        }
        set
        {
            if ((uint)index >= (uint)_length) throw new ArgumentOutOfRangeException(nameof(index));
            _items[index] = value;
        }
    }

    private void EnsureCapacity(int capacity)
    {
        if (capacity < Capacity)
        {
            return;
        }
        
        var newCapacity = capacity * 2;
        Array.Resize(ref _items, newCapacity);
    }
    
    public void Add(T item)
    {
        EnsureCapacity(_length + 1);
        _items[_length++] = item;
    }

    /// <summary>
    /// Sets the capacity to the actual number of elements in the List<T>, if that number is
    /// less than a threshold value.
    /// </summary>
    public void TrimExcess()
    {
        Array.Resize(ref _items, _length);
    }

}