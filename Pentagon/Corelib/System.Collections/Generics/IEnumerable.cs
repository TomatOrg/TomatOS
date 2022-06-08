namespace System.Collections.Generics;

/// <summary>
/// Exposes the enumerator, which supports a simple iteration over a collection of a
/// specified type.
/// </summary>
/// <typeparam name="T">The type of objects to enumerate.</typeparam>
public interface IEnumerable<out T>
{
    
    /// <summary>
    /// Returns an enumerator that iterates through the collection.
    /// </summary>
    /// <returns>An enumerator that can be used to iterate through the collection.</returns>
    public IEnumerator<T> GetEnumerator();

}