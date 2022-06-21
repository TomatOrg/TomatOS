namespace System.Threading;

public class EventWaitHandle : WaitHandle
{

    internal EventResetMode _mode;
    
    public EventWaitHandle(bool initialState, EventResetMode mode)
    {
        Create(1);

        if (mode != EventResetMode.AutoReset && mode != EventResetMode.ManualReset)
        {
            throw new ArgumentException("Value of flags is invalid.", nameof(mode));
        }
        
        _mode = mode;
        if (initialState)
        {
            Set();
        }
    }

    public override bool WaitOne()
    {
        var success = WaitOneInternal();
        if (_mode == EventResetMode.ManualReset) 
            Set();
        return success;
    }

    public override bool WaitOne(TimeSpan timeout)
    {
        var success = WaitOneInternal(timeout);
        if (_mode == EventResetMode.ManualReset) 
            Set();
        return success;
    }

    public bool Set()
    {
        if (Waitable == 0) 
            throw new ObjectDisposedException();
        
        return WaitableSend(Waitable, false);
    }

    public bool Reset()
    {
        if (Waitable == 0) 
            throw new ObjectDisposedException();
        
        return WaitableWait(Waitable, false) == 2;
    }

}