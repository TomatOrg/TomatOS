using System.Runtime.CompilerServices;
using System.Runtime.Intrinsics.X86;

namespace System.Threading;

public delegate void ParameterizedThreadStart(object obj);

public delegate void ThreadStart();

public sealed class Thread
{

    // itay: @StaticSaga choose a value for OptimalMaxSpinWaitsPerSpinIteration 
    // StaticSaga: 1000
    public static int OptimalMaxSpinWaitsPerSpinIteration => 1000;
    
    public static extern Thread CurrentThread
    {
        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        get;
    }

    public bool IsAlive => (GetNativeThreadState(_threadHandle) & 0xFFF) < 5;

    private string _name = null;
    public string Name
    {
        get => _name;
        set
        {
            if (_name != null)
                throw new InvalidOperationException("the Name property has already been set.");
            _name = value;
            SetNativeThreadName(_threadHandle, _name);
        }
    }

    public ThreadState ThreadState
    {
        get
        {
            var state = GetNativeThreadState(_threadHandle);
            var threadState = state switch
            {
                0 => ThreadState.Unstarted,
                1 => ThreadState.Running,
                2 => ThreadState.Running,
                // TODO: we need a way to figure out how to set the thread 
                //       as suspended if we entered the wait from the suspended
                //       state, we could always just track it in here
                3 => ThreadState.WaitSleepJoin,
                4 => ThreadState.Suspended,
                5 => ThreadState.Stopped,
            };
            
            // we can track the suspend request state
            threadState |= (state & 0x1000) == 0 ? 0 : ThreadState.SuspendRequested;
            
            return threadState;
        }
    }
    
    private ulong _threadHandle;

    public Thread(ParameterizedThreadStart start)
    {
        if (start == null)
            throw new ArgumentNullException(nameof(start));

        _threadHandle = CreateNativeThread(start, this);
    }

    public Thread(ThreadStart start)
    {
        if (start == null)
            throw new ArgumentNullException(nameof(start));

        _threadHandle = CreateNativeThread(start, this);
    }

    ~Thread()
    {
        ReleaseNativeThread(_threadHandle);
    }

    public void Start(object parameter = null)
    {
        StartNativeThread(_threadHandle, parameter);
    }

    public static void Sleep(int millisecondsTimeout)
    {
        if (millisecondsTimeout < -1)
            throw new ArgumentOutOfRangeException(nameof(millisecondsTimeout), "Number must be either non-negative and less than or equal to Int32.MaxValue or -1.");
        
        Sleep(new TimeSpan(millisecondsTimeout * TimeSpan.TicksPerMillisecond));
    }

    public static void Sleep(TimeSpan timeout)
    {
        var waitable = WaitHandle.WaitableAfter(timeout.Ticks / TimeSpan.TicksPerMillisecond * 1000);
        WaitHandle.WaitableWait(waitable, true);
        WaitHandle.ReleaseWaitable(waitable);
    }

    public static void SpinWait(int iterations)
    {
        for (var i = 0; i < iterations; i++)
        {
            X86Base.Pause();
        }
    }

    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    public static extern int GetCurrentProcessorId();

    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    public static extern bool Yield();
    
    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    private static extern int GetNativeThreadState(ulong thread);
    
    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    private static extern ulong CreateNativeThread(Delegate start, Thread managedThread);

    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    private static extern void StartNativeThread(ulong thread, object parameter);
    
    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    private static extern void ReleaseNativeThread(ulong thread);
    
    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    private static extern void SetNativeThreadName(ulong thread, string name);

}