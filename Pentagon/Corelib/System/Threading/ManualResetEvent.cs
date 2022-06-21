namespace System.Threading;

public sealed class ManualResetEvent : EventWaitHandle
{

    public ManualResetEvent(bool initialState)
        : base(initialState, EventResetMode.ManualReset)
    {
    }

    public override bool WaitOne()
    {
        var success = WaitOneInternal();
        Set();
        return success;
    }
    
    public override bool WaitOne(TimeSpan timeout)
    {
        var success = WaitOneInternal(timeout);
        Set();
        return success;
    }

}