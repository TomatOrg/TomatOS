using System.Runtime.CompilerServices;

namespace System.Threading;

public static class Monitor
{

    public static void Enter(object obj)
    {
        var lockTaken = false;
        Enter(obj, ref lockTaken);   
    }

    public static void Enter(object obj, ref bool lockTaken)
    {
        if (lockTaken)
            throw new ArgumentException("Argument must be initialized to false", nameof(lockTaken));

        if (obj == null)
            throw new ArgumentNullException(nameof(obj));

        switch (EnterInternal(obj, ref lockTaken))
        {
            case 0: return;
            case 3: throw new OutOfMemoryException();
            default: throw new SystemException();
        }
    }

    public static void Exit(object obj)
    {
        if (obj == null)
            throw new ArgumentNullException(nameof(obj));

        switch (ExitInternal(obj))
        {
            case 0: return;
            case 3: throw new OutOfMemoryException();
            case 6: throw new SynchronizationLockException();
            default: throw new SystemException();
        }
    }

    
    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    public static extern bool IsEntered(object obj);

    public static void Pulse(object obj)
    {
        if (obj == null)
            throw new ArgumentNullException(nameof(obj));

        switch (PulseInternal(obj))
        {
            case 0: return;
            case 3: throw new OutOfMemoryException();
            case 6: throw new SynchronizationLockException();
            default: throw new SystemException();
        }
    }

    public static void PulseAll(object obj)
    {
        if (obj == null)
            throw new ArgumentNullException(nameof(obj));

        switch (PulseAllInternal(obj))
        {
            case 0: return;
            case 3: throw new OutOfMemoryException();
            case 6: throw new SynchronizationLockException();
            default: throw new SystemException();
        }
    }


    public static bool Wait(object obj)
    {
        if (obj == null)
            throw new ArgumentNullException(nameof(obj));

        switch (WaitInternal(obj))
        {
            case 0: return true;
            case 3: throw new OutOfMemoryException();
            case 6: throw new SynchronizationLockException();
            default: throw new SystemException();
        }
    }

    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    private static extern int EnterInternal(object obj, ref bool lockTaken);

    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    private static extern int ExitInternal(object obj);

    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    private static extern int PulseInternal(object obj);

    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    private static extern int PulseAllInternal(object obj);

    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    private static extern int WaitInternal(object obj);

}