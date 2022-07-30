namespace System.Collections.Generic;

// Provides a read-only, covariant view of a generic list.
public interface IReadOnlyList<out T> : IReadOnlyCollection<T>
{
    T this[int index] { get; }
}