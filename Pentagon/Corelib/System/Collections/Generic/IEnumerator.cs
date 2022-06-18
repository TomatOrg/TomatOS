namespace System.Collections.Generic;

public interface IEnumerator<out T> : IEnumerator, IDisposable
{
    
    public new T Current { get; }
    
}