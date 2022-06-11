namespace System.Collections.Generic;

public interface ICollection<T> : IEnumerable<T>
{
    
    public int Count { get; }
    
    public bool IsReadOnly { get; }

    public void Add(T item);

    public void Clear();

    public bool Contains(T item);

    public void CopyTo(T[] array, int arrayIndex);

    public bool Remove(T item);

}