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
        else
        {
            Reset();
        }
    }

    #region Auto Reset

    internal bool SetAutoReset()
    {
        return WaitableSend(Waitable, false);
    }

    internal bool ResetAutoReset()
    {
        return WaitableWait(Waitable, false) == 2;
    }

    #endregion

    #region Manual Reset

    
    internal bool SetManualReset()
    {
        // check the waitable is not closed already, if it is then there
        // is nothing to do
        if (WaitableWait(Waitable, false) == 1)
        {
            return false;
        }
        
        // close the waitable, waking all waiters
        WaitableClose(Waitable);

        return true;
    }

    internal bool ResetManualReset()
    {
        // check the waitable is closed, if not then there
        // is nothing  to do on the reset 
        if (WaitableWait(Waitable, false) != 1)
        {
            return false;
        }
        
        // create a new one to replace the old one 
        var waitable = CreateWaitable(1);
        
        // Exchange it atomically, this will make sure that from now only the new event, which
        // is not closed, is listened to 
        var oldWaitable = Interlocked.Exchange(ref Waitable, waitable);
        
        // release the reference we had, anything waiting on this will also release 
        // its own reference soon enough and then the handle will close
        ReleaseWaitable(oldWaitable);

        return true;
    }


    #endregion
    
    public virtual bool Set()
    {
        if (Waitable == 0) 
            throw new ObjectDisposedException();

        return _mode == EventResetMode.AutoReset ? SetAutoReset() : SetManualReset();
    }

    public virtual bool Reset()
    {
        if (Waitable == 0) 
            throw new ObjectDisposedException();

        return _mode == EventResetMode.AutoReset ? ResetAutoReset() : ResetManualReset();
    }

}