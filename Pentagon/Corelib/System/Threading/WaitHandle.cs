using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace System.Threading;

public abstract class WaitHandle : IDisposable
{

    internal ulong Waitable = 0;

    // don't allow anyone but ourselves to inherit this
    internal WaitHandle()
    {
    }

    internal void Create(int count)
    {
        Waitable = CreateWaitable(count);
    }

    ~WaitHandle()
    {
        Dispose(false);
    }
    
    public virtual void Close()
    {
        Dispose();
    }
    
    public void Dispose()
    {
        Dispose(true);
        GC.SuppressFinalize(this);
    }

    protected virtual void Dispose(bool disposing)
    {
        if (Waitable == 0)
        {
            return;
        }
        
        ReleaseWaitable(Waitable);
        Waitable = 0;
    }

    #region Wait One
    
    public virtual bool WaitOne()
    {
        return WaitOneInternal();
    }

    public virtual bool WaitOne(int millisecondsTimeout)
    {
        if (millisecondsTimeout == -1)
            return WaitOne();

        if (millisecondsTimeout < 0)
            throw new ArgumentOutOfRangeException(nameof(millisecondsTimeout), "Number must be either non-negative and less than or equal to Int32.MaxValue or -1.");

        return WaitOne(new TimeSpan(millisecondsTimeout * TimeSpan.TicksPerMillisecond));
    }

    public virtual bool WaitOne(TimeSpan timeout)
    {
        return WaitOneInternal(timeout);
    }

    internal bool WaitOneInternal()
    {
        if (Waitable == 0) 
            throw new ObjectDisposedException();
        
        return WaitableWait(Waitable, true) == 2;
    }

    internal bool WaitOneInternal(TimeSpan timeout)
    {
        if (Waitable == 0) 
            throw new ObjectDisposedException();

        // get the waitable, increase ref-count
        var waitable = PutWaitable(Waitable);

        // create the timeout, we take the value in imcroseconds
        var timeoutWaitable = WaitableAfter(timeout.Ticks / TimeSpan.TicksPerMillisecond * 1000);
                
        // wait on both of them
        var selected = WaitableSelect2(waitable, timeoutWaitable, true);

        // release both refs
        ReleaseWaitable(timeoutWaitable);
        ReleaseWaitable(waitable);

        return selected == 0;
    }

    #endregion

    #region Native

    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    internal static extern bool WaitableSend(ulong waitable, bool block);

    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    internal static extern int WaitableWait(ulong waitable, bool block);

    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    internal static extern void WaitableClose(ulong waitable);

    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    internal static extern int WaitableSelect2(ulong waitable1, ulong waitable2, bool block);

    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    internal static extern ulong CreateWaitable(int count);

    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    internal static extern ulong WaitableAfter(long timeoutMicro);

    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    internal static extern ulong PutWaitable(ulong waitable);

    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    internal static extern void ReleaseWaitable(ulong waitable);

    #endregion

}