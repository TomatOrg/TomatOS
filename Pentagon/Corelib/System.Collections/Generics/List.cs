namespace System.Collections.Generics;

public class List<T> : IEnumerable<T>, IList<T>
{

    private T[] _buffer;
    private int _length;
    
    public int Count => _length;
    public bool IsReadOnly => false;

    private void ArrayGrow(int addLen, int minCap)
    {
        var minLen = _length + addLen;
        var cap = _buffer == null ? _buffer.Length : 0;

        // compute the minimum capacity needed
        if (minLen > minCap)
        {
            minCap = minLen;
        }

        if (minCap <= cap)
        {
            return;
        }
        
        // increase needed capacity to guarantee O(1) amortized
        if (minCap < 2 * cap)
        {
            minCap = 2 * cap;
        } else if (minCap < 4)
        {
            minCap = 4;
        }

        if (_buffer != null)
        {
            var old = _buffer;
            _buffer = new T[minCap];
            for (var i = 0; i < _length; i++)
            {
                _buffer[i] = old[i];
            }
        }
        else
        {
            _buffer = new T[minCap];
        }
    }
    
    private void ArrayMaybeGrow(int n)
    {
        if (_buffer == null || _length + 1 > _buffer.Length)
        {
            ArrayGrow(n, 0);
        }
    }
    
    public void Add(T item)
    {
        ArrayMaybeGrow(1);
        _buffer[_length++] = item;
    }

    public void Clear()
    {
        _length = 0;
    }

    public bool Contains(T item)
    {
        return IndexOf(item) >= 0;
    }

    public void CopyTo(T[] array, int arrayIndex)
    {
        if (array == null) throw new ArgumentNullException(nameof(array));
        if (arrayIndex < 0) throw new ArgumentOutOfRangeException(nameof(arrayIndex));
        if (array.Length - arrayIndex < _length) throw new ArgumentException();
        
        for (var i = 0; i < _length; i++)
        {
            array[arrayIndex + i] = _buffer[i];
        }
    }

    public bool Remove(T item)
    {
        var idx = IndexOf(item);
        if (idx < 0)
        {
            return false;
        }
        RemoveAt(idx);
        return true;
    }

    public T this[int index]
    {
        get
        {
            if (index >= _length) throw new ArgumentOutOfRangeException(nameof(index));
            return _buffer[index];
        }
        set
        {
            if (index >= _length) throw new ArgumentOutOfRangeException(nameof(index));
            _buffer[index] = value;
        }
    }

    public int IndexOf(T item)
    {
        for (var i = 0; i < _length; i++)
        {
            if (_buffer[i].Equals(item))
            {
                return i;
            }
        }

        return -1;
    }

    public void Insert(int index, T item)
    {
        if (index >= _length) throw new ArgumentOutOfRangeException(nameof(index));
        
        ArrayMaybeGrow(1);
        for (var i = _length - 1; i >= index; i++)
        {
            _buffer[i + 1] = _buffer[i];
        }
        _buffer[index] = item;
        _length += 1;
    }

    public void RemoveAt(int index)
    {
        if (index >= _length) throw new ArgumentOutOfRangeException(nameof(index));
        ArrayMaybeGrow(1);
        for (var i = index; i < _length - index; i++)
        {
            _buffer[i] = _buffer[i + 1];
        }
        _length -= 1;
    }

    public IEnumerator<T> GetEnumerator()
    {
        throw new NotImplementedException();
    }
}