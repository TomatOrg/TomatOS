namespace System.Threading;

public class SynchronizationLockException : SystemException
{
    
    public SynchronizationLockException()
        : base("Object synchronization method was called from an unsynchronized block of code.")
    {
    }

    public SynchronizationLockException(string message)
        : base(message)
    {
    }

    public SynchronizationLockException(string message, Exception innerException)
        : base(message, innerException)
    {
    }
    
}