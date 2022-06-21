namespace System.Threading;

public sealed class AutoResetEvent : EventWaitHandle
{

    public AutoResetEvent(bool initialState)
        : base(initialState, EventResetMode.AutoReset)
    {
    }

    public override bool WaitOne()
    {
        return WaitOneInternal();
    }

    public override bool WaitOne(TimeSpan timeout)
    {
        return WaitOneInternal(timeout);
    }
}