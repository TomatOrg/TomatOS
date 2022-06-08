namespace System.Collections.Generics;

/// <summary>
/// Represents a collection of objects that can be individually accessed by index.
/// </summary>
/// <typeparam name="T">The type of elements in the list.</typeparam>
public interface IList<T> : ICollection<T>, IEnumerable<T>
{
    
    /// <summary>
    /// Gets or sets the element at the specified index.
    /// </summary>
    /// <param name="index">The zero-based index of the element to get or set.</param>
    public T this[int index] { get; set; }

    /// <summary>
    /// Determines the index of a specific item in the IList.
    /// </summary>
    /// <param name="item">The object to locate in the IList.</param>
    /// <returns>The index of item if found in the list; otherwise, -1.</returns>
    public int IndexOf(T item);

    /// <summary>
    /// Inserts an item to the IList at the specified index.
    /// </summary>
    /// <param name="index">The zero-based index at which item should be inserted.</param>
    /// <param name="item">The object to insert into the IList.</param>
    public void Insert(int index, T item);

    /// <summary>
    /// Removes the IList item at the specified index.
    /// </summary>
    /// <param name="index">The zero-based index of the item to remove.</param>
    public void RemoveAt(int index);

}