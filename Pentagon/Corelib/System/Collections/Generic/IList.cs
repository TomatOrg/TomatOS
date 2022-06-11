namespace System.Collections.Generic;

public interface IList<T> : ICollection<T>
{
    
    public T this[int index] { get; set; }

    public int IndexOf(T item);

    public void Insert(int index, T item);

    public void RemoveAt(int index);

}