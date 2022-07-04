namespace System.Threading;

public sealed class AutoResetEvent : EventWaitHandle
{

    public AutoResetEvent(bool initialState)
        : base(initialState, EventResetMode.AutoReset)
    {
    }

    public override bool Set()
    {
        return SetAutoReset();
    }

    public override bool Reset()
    {
        return ResetAutoReset();
    }
    
}