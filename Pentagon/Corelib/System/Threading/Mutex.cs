namespace System.Threading;

public sealed class Mutex : WaitHandle
{
    
    public Mutex(bool initiallyOwned = false) 
    {
        Create(1);
        
        if (!initiallyOwned)
        {
            WaitableSend(Waitable, true);
        }
    }

    public void ReleaseMutex()
    {
        if (Waitable == 0)
            throw new ObjectDisposedException();

        if (!WaitableSend(Waitable, false))
        {
            throw new ApplicationException(
                "Object synchronization method was called from an unsynchronized block of code.");
        }
    }
    
}