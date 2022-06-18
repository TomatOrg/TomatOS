using System.Runtime.CompilerServices;

namespace System.Collections.Generic;

public class List<T> : IList<T>
{
    
    private const int DefaultCapacity = 4;
    
    private T[] _items;
    private int _version;

    public int Capacity
    {
        get => _items.Length;
        set
        {
            // make sure the value is valid
            if (value < Count)
                throw new ArgumentOutOfRangeException(nameof(value), "capacity was less than the current size.");

            // no need to allocate anything new
            if (value == _items.Length) 
                return;
            
            // check if we need to allocate even
            if (value > 0)
            {
                var newItems = new T[value];
                if (Count > 0)
                {
                    Array.Copy(_items, newItems, Count);
                }
                _items = newItems;
            }
            else
            {
                _items = Array.Empty<T>();
            }
        }
    }

    public int Count { get; private set; } = 0;
    public bool IsReadOnly { get; }

    public List()
    {
        _items = Array.Empty<T>();
    }

    public List(int capacity)
    {
        if (capacity < 0) throw new ArgumentOutOfRangeException(nameof(capacity), "Non-negative number required.");
        _items = capacity == 0 ? Array.Empty<T>() : new T[capacity];
    }

    public T this[int index]
    {
        get
        {
            if ((uint)index >= (uint)Count) 
                throw new ArgumentOutOfRangeException(nameof(index), "Index was out of range. Must be non-negative and less than the size of the collection.");
            return _items[index];
        }
        set
        {
            if ((uint)index >= (uint)Count) 
                throw new ArgumentOutOfRangeException(nameof(index), "Index was out of range. Must be non-negative and less than the size of the collection.");
            _items[index] = value;
            _version++;
        }
    }

    public int IndexOf(T item)
    {
        return Array.IndexOf(_items, item, 0, Count);
    }

    public void Insert(int index, T item)
    {
        if ((uint)index > (uint)Count)
            throw new ArgumentOutOfRangeException(nameof(index), "Index must be within the bounds of the List.");

        if (Count < _items.Length)
        {
            Grow(Count + 1);
        }

        if (index < Count)
        {
            Array.Copy(_items, index, _items, index + 1, Count - index);
        }

        _version++;
        Count++;
        _items[index] = item;
    }

    public void RemoveAt(int index)
    {
        if ((uint)index >= (uint)Count)
            throw new ArgumentOutOfRangeException(nameof(index), "Index was out of range. Must be non-negative and less than the size of the collection.");
        _version++;
        Count--;
        if (index < Count)
        {
            Array.Copy(_items, index + 1, _items, index, Count - index);
        }
        _items[Count] = default;
    }

    private void Grow(int capacity)
    {
        var newCapacity = _items.Length == 0 ? DefaultCapacity : 2 * _items.Length;

        if ((uint)newCapacity > Array.MaxLength)
        {
            newCapacity = Array.MaxLength;
        }

        if (newCapacity < capacity)
        {
            newCapacity = capacity;
        }

        Capacity = newCapacity;
    }
    
    public void Add(T item)
    {
        _version++;
        var array = _items;
        var size = Count;
        if ((uint)size < (uint)array.Length)
        {
            Count = size + 1;
            array[size] = item;
        }
        else
        {
            AddWithResize(item);
        }
    }

    public void Clear()
    {
        _version++;
        // TODO: only need to clear if we have reference types
        if (Count > 0)
        {
            Array.Clear(_items, 0, Count);
        }
        Count = 0;
    }

    public bool Contains(T item)
    {
        return Count != 0 && IndexOf(item) >= 0;
    }

    public void CopyTo(T[] array, int arrayIndex)
    {
        throw new NotImplementedException();
    }

    public int EnsureCapacity(int capacity)
    {
        if (capacity < 0)
            throw new ArgumentOutOfRangeException("Non-negative number required.");

        if (Capacity < capacity)
        {
            Grow(capacity);
        }
        
        return _items.Length;
    }

    public bool Remove(T item)
    {
        var index = IndexOf(item);
        if (index >= 0)
        {
            RemoveAt(index);
            return true;
        }
        return false;
    }

    private void AddWithResize(T item)
    {
        Grow(Count + 1);
        _items[Count++] = item;
    }

    public Enumerator GetEnumerator()
    {
        return new Enumerator(this);
    }
    
    IEnumerator<T> IEnumerable<T>.GetEnumerator()
    {
        return new Enumerator(this);
    }

    IEnumerator IEnumerable.GetEnumerator()
    {
        return new Enumerator(this);
    }

    public struct Enumerator : IEnumerator<T>
    {

        private readonly List<T> _list;
        private int _index;
        private readonly int _version;

        public T Current { get; private set; }
        
        object IEnumerator.Current
        {
            get
            {
                if (_index == 0 || _index == _list.Count + 1)
                {
                    throw new InvalidOperationException("Enumeration has either not started or has already finished.");
                }
                return Current;
            }   
        }

        internal Enumerator(List<T> list)
        {
            _list = list;
            _index = 0;
            _version = list._version;
            Current = default;
        }

        public void Dispose()
        {
        }

        private bool MoveNextRare()
        {
            if (_version != _list._version)
            {
                throw new InvalidOperationException("Collection was modified; enumeration operation may not execute.");
            }
            _index = _list.Count + 1;
            Current = default;
            return false;
        }
        
        public bool MoveNext()
        {
            if (_version == _list._version && (uint)_index < (uint)_list.Count)
            {
                Current = _list._items[_index];
                _index++;
                return true;
            }
            return MoveNextRare();
        }

        public void Reset()
        {
            if (_version != _list._version)
                throw new InvalidOperationException("Collection was modified; enumeration operation may not execute.");
            _index = 0;
            Current = default;
        }
    }

}