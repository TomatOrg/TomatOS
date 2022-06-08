namespace System.Collections.Generics;

/// <summary>
/// Supports a simple iteration over a generic collection.
/// </summary>
/// <typeparam name="T">The type of objects to enumerate.</typeparam>
public interface IEnumerator<out T> : IEnumerator
{
    
    /// <summary>
    /// Gets the element in the collection at the current position of the enumerator.
    /// </summary>
    public new T Current { get; }
    
}