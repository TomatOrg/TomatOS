namespace Pentagon
{
    internal struct Semaphore
    {

        private int _value;
        private Spinlock _lock;
        private unsafe void* _waiters;
        private int _nwait;

    }
}