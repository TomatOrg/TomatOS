namespace System.Threading;

public class SemaphoreFullException : SystemException
{
    
    public SemaphoreFullException()
        : base("Adding the specified count to the semaphore would cause it to exceed its maximum count.")
    {
    }

    public SemaphoreFullException(string message)
        : base(message)
    {
    }

    public SemaphoreFullException(string message, Exception innerException)
        : base(message, innerException)
    {
    }
    
}