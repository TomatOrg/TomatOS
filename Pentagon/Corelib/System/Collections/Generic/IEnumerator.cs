namespace System.Collections.Generic;

public interface IEnumerator<out T> : IEnumerator
{
    
    public new T Current { get; }
    
}