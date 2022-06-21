namespace System.Threading;

public sealed class Semaphore : WaitHandle
{
    
    public Semaphore(int initialCount, int maximumCount)
    {
        if (initialCount < 0)
            throw new ArgumentOutOfRangeException(nameof(initialCount), "Non-negative number required.");
        
        if (maximumCount < 1)
            throw new ArgumentOutOfRangeException(nameof(initialCount), "Positive number required.");
        
        if (initialCount > maximumCount)
            throw new ArgumentException("The initial count for the semaphore must be greater than or equal to zero and less than the maximum count.");
            
        Create(maximumCount);

        if (initialCount > 0)
        {
            Release(initialCount);
        }
    }
    
    // TODO: we don't actually track currently the count, so we are going 
    //       to just ignore the return for the release in the semaphore
    public void Release(int releaseCount = 1)
    {
        if (releaseCount < 1)
            throw new ArgumentOutOfRangeException(nameof(releaseCount), "Non-negative number required.");

        for (var i = 0; i < releaseCount; i++)
        {
            if (!WaitableSend(Waitable, false))
            {
                throw new SemaphoreFullException();
            }
        }
    }
    
}