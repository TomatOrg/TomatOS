namespace System.Threading;

public sealed class ManualResetEvent : EventWaitHandle
{

    public ManualResetEvent(bool initialState)
        : base(initialState, EventResetMode.ManualReset)
    {
    }

    public override bool Set()
    {
        return SetManualReset();
    }

    public override bool Reset()
    {
        return ResetManualReset();
    }

}