using System.Collections.Generic;

namespace System.Collections.ObjectModel;

public class ReadOnlyCollection<T> : IList<T>, IReadOnlyList<T>
{
    private readonly IList<T> _list;

    public ReadOnlyCollection(IList<T> list)
    {
        _list = list ?? throw new ArgumentNullException(nameof(list));
    }

    public int Count => _list.Count;

    public T this[int index] => _list[index];

    public bool Contains(T value)
    {
        return _list.Contains(value);
    }

    public void CopyTo(T[] array, int index)
    {
        _list.CopyTo(array, index);
    }

    public IEnumerator<T> GetEnumerator()
    {
        return _list.GetEnumerator();
    }

    public int IndexOf(T value)
    {
        return _list.IndexOf(value);
    }

    protected IList<T> Items => _list;

    bool ICollection<T>.IsReadOnly => true;

    T IList<T>.this[int index]
    {
        get => _list[index];
        set => throw new NotSupportedException(NotSupportedException.ReadOnlyCollection);
    }

    void ICollection<T>.Add(T value)
    {
        throw new NotSupportedException(NotSupportedException.ReadOnlyCollection);
    }

    void ICollection<T>.Clear()
    {
        throw new NotSupportedException(NotSupportedException.ReadOnlyCollection);
    }

    void IList<T>.Insert(int index, T value)
    {
        throw new NotSupportedException(NotSupportedException.ReadOnlyCollection);
    }

    bool ICollection<T>.Remove(T value)
    {
        throw new NotSupportedException(NotSupportedException.ReadOnlyCollection);
    }

    void IList<T>.RemoveAt(int index)
    {
        throw new NotSupportedException(NotSupportedException.ReadOnlyCollection);
    }

    IEnumerator IEnumerable.GetEnumerator()
    {
        return ((IEnumerable)_list).GetEnumerator();
    }
}