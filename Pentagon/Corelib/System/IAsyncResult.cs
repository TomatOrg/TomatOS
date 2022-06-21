using System.Threading;

namespace System;

public interface IAsyncResult
{
    
    public object AsyncState { get; }
    public WaitHandle AsyncWaitHandle { get; }
    public bool CompletedSynchronously { get; }
    public bool IsCompleted { get; }
    
}