namespace System.Collections.Generic;

public interface IEnumerable<out T>
{

    public IEnumerator<T> GetEnumerator();

}