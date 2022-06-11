namespace System.Collections;

public interface IEnumerator
{
    
    public object Current { get; }

    public bool MoveNext();

    public void Reset();

}