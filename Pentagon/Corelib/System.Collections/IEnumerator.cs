namespace System.Collections;

/// <summary>
/// Supports a simple iteration over a non-generic collection.
/// </summary>
public interface IEnumerator
{
    
    /// <summary>
    /// Gets the element in the collection at the current position of the enumerator.
    /// </summary>
    public object Current { get; }

    /// <summary>
    /// Advances the enumerator to the next element of the collection.
    /// </summary>
    /// <returns>
    /// true if the enumerator was successfully advanced to the next element; false if the
    /// enumerator has passed the end of the collection.
    /// </returns>
    public bool MoveNext();

    /// <summary>
    /// Sets the enumerator to its initial position, which is before the first element in the
    /// collection.
    /// </summary>
    public void Reset();

}