namespace System.Collections.Generic;

public interface IEnumerable<out T> : IEnumerable
{

    public new IEnumerator<T> GetEnumerator();

}