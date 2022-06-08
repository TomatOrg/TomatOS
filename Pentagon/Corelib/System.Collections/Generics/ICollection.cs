namespace System.Collections.Generics;

/// <summary>
/// Defines methods to manipulate generic collections.
/// </summary>
public interface ICollection<T> : IEnumerable<T>
{
    
    /// <summary>
    /// Gets the number of elements contained in the ICollection.
    /// </summary>
    public int Count { get; }
    
    /// <summary>
    /// Gets a value indicating whether the ICollection is read-only.
    /// </summary>
    public bool IsReadOnly { get; }

    /// <summary>
    /// Adds an item to the ICollection.
    /// </summary>
    /// <param name="item">The object to add to the ICollection.</param>
    public void Add(T item);

    /// <summary>
    /// Removes all items from the ICollection.
    /// </summary>
    public void Clear();

    /// <summary>
    /// Determines whether the ICollection contains a specific value.
    /// </summary>
    /// <param name="item">The object to locate in the ICollection.</param>
    /// <returns>true if item is found in the ICollection; otherwise, false.</returns>
    public bool Contains(T item);

    /// <summary>
    /// Copies the elements of the ICollection to an Array, starting at a particular Array
    /// index.
    /// </summary>
    /// <param name="array">
    /// The one-dimensional Array that is the destination of the elements copied from
    /// ICollection. The Array must have zero-based indexing.
    /// </param>
    /// <param name="arrayIndex">
    /// The zero-based index in array at which copying begins.
    /// </param>
    public void CopyTo(T[] array, int arrayIndex);

    /// <summary>
    /// Removes the first occurrence of a specific object from the ICollection.
    /// </summary>
    /// <param name="item">The object to remove from the ICollection</param>
    /// <returns>
    /// true if item was successfully removed from the ICollection; otherwise, false.
    /// This method also returns false if item is not found in the original ICollection.
    /// </returns>
    public bool Remove(T item);

}