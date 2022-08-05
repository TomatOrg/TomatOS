// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.CompilerServices;

namespace System.Threading.Tasks
{
    internal struct VoidTaskResult { }
    public enum TaskStatus
    {
        Created,
        WaitingForActivation,
        WaitingToRun,
        WaitingForChildrenToComplete,
        RanToCompletion,
        Canceled,
        Faulted
    }
    // Task<TResult> is in Future.cs, idk why
    public class Task : IAsyncResult, IDisposable
    {
        [Flags]
        internal enum TaskStateFlags
        {
            Started = 0x10000,
            DelegateInvoked = 0x20000,
            Disposed = 0x40000,
            ExceptionObservedByParent = 0x80000,
            CancellationAcknowledged = 0x100000,
            Faulted = 0x200000,
            Canceled = 0x400000,
            WaitingOnChildren = 0x800000,
            RanToCompletion = 0x1000000,
            WaitingForActivation = 0x2000000,
            CompletionReserved = 0x4000000,
            WaitCompletionNotification = 0x10000000,
            ExecutionContextIsNull = 0x20000000,
            TaskScheduledWasFired = 0x40000000,
            CompletedMask = Canceled | Faulted | RanToCompletion,
            OptionsMask = 0xFFFF
        }
        private const int CANCELLATION_REQUESTED = 0x1;

        // TODO: [ThreadLocal]
        internal static Task? t_currentTask;

        internal ContingentProperties? m_contingentProperties;
        internal Delegate? m_action;
        internal object? m_stateObject;
        internal TaskScheduler? m_taskScheduler;
        internal int m_stateFlags;
        private volatile object? m_continuationObject;
        private static readonly object s_taskCompletionSentinel = new object();
        public bool IsCompleted { get => (m_stateFlags & (int)TaskStateFlags.CompletedMask) != 0; }
        public object? AsyncState => m_stateObject;
        bool IAsyncResult.CompletedSynchronously => false;
        public TaskAwaiter GetAwaiter() => new TaskAwaiter(this);
        public static YieldAwaitable Yield() => default;
        public bool IsCanceled =>
           (m_stateFlags & ((int)TaskStateFlags.Canceled | (int)TaskStateFlags.Faulted)) == (int)TaskStateFlags.Canceled;
        internal bool IsCancellationRequested
        {
            get
            {
                // TODO:
                return false;
            }
        }

        internal static readonly Task<VoidTaskResult> s_cachedCompleted = new Task<VoidTaskResult>(false, default, (TaskCreationOptions)InternalTaskOptions.DoNotDispose, default);
        public static Task CompletedTask => s_cachedCompleted;

        public TaskCreationOptions CreationOptions => Options & (TaskCreationOptions)(~InternalTaskOptions.InternalOptionsMask);
        internal TaskScheduler? ExecutingTaskScheduler => m_taskScheduler;


        // Special internal constructor to create an already-completed task.
        // if canceled==true, create a Canceled task, or else create a RanToCompletion task.
        // Constructs the task as already completed
        internal Task(bool canceled, TaskCreationOptions creationOptions, CancellationToken ct)
        {
            int optionFlags = (int)creationOptions;
            if (canceled)
            {
                m_stateFlags = (int)TaskStateFlags.Canceled | (int)TaskStateFlags.CancellationAcknowledged | optionFlags;
                m_contingentProperties = new ContingentProperties() // can't have children, so just instantiate directly
                {
                    m_cancellationToken = ct,
                    m_internalCancellationRequested = CANCELLATION_REQUESTED,
                };
            }
            else
            {
                m_stateFlags = (int)TaskStateFlags.RanToCompletion | optionFlags;
            }
        }

        internal Task()
        {
            m_stateFlags = (int)TaskStateFlags.WaitingForActivation | (int)InternalTaskOptions.PromiseTask;
        }

        internal Task(object? state, TaskCreationOptions creationOptions, bool promiseStyle)
        {
            // Only set a parent if AttachedToParent is specified.
            if ((creationOptions & TaskCreationOptions.AttachedToParent) != 0)
            {
                Task? parent = Task.InternalCurrent;
                if (parent != null)
                {
                    EnsureContingentPropertiesInitializedUnsafe().m_parent = parent;
                }
            }

            TaskConstructorCore(null, state, default, creationOptions, InternalTaskOptions.PromiseTask, null);
        }

        internal Task(Delegate action, object? state, Task? parent, CancellationToken cancellationToken,
            TaskCreationOptions creationOptions, InternalTaskOptions internalOptions, TaskScheduler? scheduler)
        {

            // Keep a link to the parent if attached
            if (parent != null && (creationOptions & TaskCreationOptions.AttachedToParent) != 0)
            {
                EnsureContingentPropertiesInitializedUnsafe().m_parent = parent;
            }

            TaskConstructorCore(action, state, cancellationToken, creationOptions, internalOptions, scheduler);

            //CapturedContext = ExecutionContext.Capture();
        }

        internal void TaskConstructorCore(Delegate? action, object? state, CancellationToken cancellationToken,
            TaskCreationOptions creationOptions, InternalTaskOptions internalOptions, TaskScheduler? scheduler)
        {
            m_action = action;
            m_stateObject = state;
            m_taskScheduler = scheduler;

            // Check for validity of options
            if ((creationOptions &
                    ~(TaskCreationOptions.AttachedToParent |
                      TaskCreationOptions.LongRunning |
                      TaskCreationOptions.DenyChildAttach |
                      TaskCreationOptions.HideScheduler |
                      TaskCreationOptions.PreferFairness |
                      TaskCreationOptions.RunContinuationsAsynchronously)) != 0)
            {
                //ThrowHelper.ThrowArgumentOutOfRangeException(ExceptionArgument.creationOptions);
            }



            int tmpFlags = (int)creationOptions | (int)internalOptions; // one write to the volatile m_stateFlags instead of two when setting the above options
            m_stateFlags = m_action == null || (internalOptions & InternalTaskOptions.ContinuationTask) != 0 ?
                tmpFlags | (int)TaskStateFlags.WaitingForActivation :
                tmpFlags;

            // Now is the time to add the new task to the children list
            // of the creating task if the options call for it.
            // We can safely call the creator task's AddNewChild() method to register it,
            // because at this point we are already on its thread of execution.

            ContingentProperties? props = m_contingentProperties;
            if (props != null)
            {
                Task? parent = props.m_parent;
                if (parent != null
                    && ((creationOptions & TaskCreationOptions.AttachedToParent) != 0)
                    && ((parent.CreationOptions & TaskCreationOptions.DenyChildAttach) == 0))
                {
                    parent.AddNewChild();
                }
            }


        }
        internal TaskCreationOptions Options => OptionsMethod(m_stateFlags);

        // Similar to Options property, but allows for the use of a cached flags value rather than
        // a read of the volatile m_stateFlags field.
        internal static TaskCreationOptions OptionsMethod(int flags)
        {
            return (TaskCreationOptions)(flags & (int)TaskStateFlags.OptionsMask);
        }

        public static Task<TResult> FromCanceled<TResult>(CancellationToken cancellationToken)
        {
            // if (!cancellationToken.IsCancellationRequested)
            //      ThrowHelper.ThrowArgumentOutOfRangeException(ExceptionArgument.cancellationToken);
            return new Task<TResult>(true, default, TaskCreationOptions.None, cancellationToken);
        }


        internal static Task? InternalCurrentIfAttached(TaskCreationOptions creationOptions)
        {
            return (creationOptions & TaskCreationOptions.AttachedToParent) != 0 ? InternalCurrent : null;
        }
        internal ContingentProperties EnsureContingentPropertiesInitialized()
        {
            return Volatile.Read(ref m_contingentProperties) ?? InitializeContingentProperties();

            ContingentProperties InitializeContingentProperties()
            {
                Interlocked.CompareExchange(ref m_contingentProperties, new ContingentProperties(), null);
                return m_contingentProperties;
            }
        }
        internal ContingentProperties EnsureContingentPropertiesInitializedUnsafe() =>
            m_contingentProperties ??= new ContingentProperties();

        internal void NotifyParentIfPotentiallyAttachedTask()
        {
            Task? parent = m_contingentProperties?.m_parent;
            if (parent != null
                 && ((parent.CreationOptions & TaskCreationOptions.DenyChildAttach) == 0)
                 && (((TaskCreationOptions)(m_stateFlags & (int)TaskStateFlags.OptionsMask)) & TaskCreationOptions.AttachedToParent) != 0)
            {
                parent.ProcessChildCompletion(this);
            }
        }
        internal void FinishContinuations()
        {
            object? continuationObject = Interlocked.Exchange(ref m_continuationObject, s_taskCompletionSentinel);

            if (continuationObject != null)
            {
                RunContinuations(continuationObject);
            } else
            {
                //Diagnostics.Log.LogObject("no continuations", this);
            }

        }


        public Task WaitAsync(CancellationToken cancellationToken) => WaitAsync(Timeout.UnsignedInfinite, cancellationToken);

        public Task WaitAsync(TimeSpan timeout) => WaitAsync(timeout, default);

        public Task WaitAsync(TimeSpan timeout, CancellationToken cancellationToken) =>
            WaitAsync(timeout, cancellationToken);

        private Task WaitAsync(uint millisecondsTimeout, CancellationToken cancellationToken)
        {
            if (IsCompleted || (!cancellationToken.CanBeCanceled && millisecondsTimeout == Timeout.UnsignedInfinite))
            {
                return this;
            }
            // TODO:
            return this;

            /*if (cancellationToken.IsCancellationRequested)
            {
                return FromCanceled(cancellationToken);
            }

            if (millisecondsTimeout == 0)
            {
                return FromException(new TimeoutException());
            }

            return new CancellationPromise<VoidTaskResult>(this, millisecondsTimeout, cancellationToken);*/
        }

        public bool IsCompletedSuccessfully => (m_stateFlags & (int)TaskStateFlags.CompletedMask) == (int)TaskStateFlags.RanToCompletion;

        private void RunContinuations(object continuationObject) // separated out of FinishContinuations to enable it to be inlined
        {
            bool canInlineContinuations = true;
            // TODO: types other than Action
            AwaitTaskContinuation.RunOrScheduleAction((Action)continuationObject, canInlineContinuations);
        }

        internal bool ExecuteEntry()
        {
            int previousState = 0;
            if (!AtomicStateUpdate((int)TaskStateFlags.DelegateInvoked,
                                    (int)TaskStateFlags.DelegateInvoked | (int)TaskStateFlags.CompletedMask,
                                    ref previousState) && (previousState & (int)TaskStateFlags.Canceled) == 0)
            {
                return false;
            }

            ExecuteWithThreadLocal(ref t_currentTask);
            
            return true;
        }

        // TODO: thread
        private void ExecuteWithThreadLocal(ref Task? currentTaskSlot)
        {
            Task? previousTask = currentTaskSlot;

            try
            {
                // place the current task into TLS.
                currentTaskSlot = this;

                InnerInvoke();
                Finish(true);
            }
            finally
            {
                currentTaskSlot = previousTask;
            }
        }
        internal virtual void InnerInvoke()
        {
            if (m_action is Action action)
            {
                action();
                return;
            }

            if (m_action is Action<object?> actionWithState)
            {
                actionWithState(m_stateObject);
                return;
            }
        }

        internal void SetContinuationForAwait(
            Action continuationAction, bool continueOnCapturedContext, bool flowExecutionContext)
        {
            if (!AddTaskContinuation(continuationAction, addBeforeOthers: false))
            {
                AwaitTaskContinuation.UnsafeScheduleAction(continuationAction, this);
            }
        }

        // Record a continuation task or action.
        // Return true if and only if we successfully queued a continuation.
        private bool AddTaskContinuation(object tc, bool addBeforeOthers)
        {
            //Debug.Assert(tc != null);

            // Make sure that, if someone calls ContinueWith() right after waiting for the predecessor to complete,
            // we don't queue up a continuation.
            if (IsCompleted)
            {
                return false;
            }

            // Try to just jam tc into m_continuationObject
            if ((m_continuationObject != null) || (Interlocked.CompareExchange(ref m_continuationObject, tc, null) != null))
            {
                // If we get here, it means that we failed to CAS tc into m_continuationObject.
                // Therefore, we must go the more complicated route.
                return AddTaskContinuationComplex(tc, addBeforeOthers);
            }
            else
            {
                //Diagnostics.Log.LogObject("added simple", this);
                return true;
            }
        }

        // Support method for AddTaskContinuation that takes care of multi-continuation logic.
        // Returns true if and only if the continuation was successfully queued.
        // THIS METHOD ASSUMES THAT m_continuationObject IS NOT NULL.  That case was taken
        // care of in the calling method, AddTaskContinuation().
        private bool AddTaskContinuationComplex(object tc, bool addBeforeOthers)
        {
            //Diagnostics.Log.LogObject("adding complex", this);

            //Debug.Assert(tc != null, "Expected non-null tc object in AddTaskContinuationComplex");

            object? oldValue = m_continuationObject;

            // Logic for the case where we were previously storing a single continuation
            if ((oldValue != s_taskCompletionSentinel) && (!(oldValue is List<object?>)))
            {
                // Construct a new TaskContinuation list and CAS it in.
                Interlocked.CompareExchange(ref m_continuationObject, new List<object?> { oldValue }, oldValue);

                // We might be racing against another thread converting the single into
                // a list, or we might be racing against task completion, so resample "list"
                // below.
            }

            // m_continuationObject is guaranteed at this point to be either a List or
            // s_taskCompletionSentinel.
            List<object?>? list = m_continuationObject as List<object?>;
            Debug.Assert((list != null) || (m_continuationObject == s_taskCompletionSentinel),
                "Expected m_continuationObject to be list or sentinel");

            // If list is null, it can only mean that s_taskCompletionSentinel has been exchanged
            // into m_continuationObject.  Thus, the task has completed and we should return false
            // from this method, as we will not be queuing up the continuation.
            if (list != null)
            {
                lock (list)
                {
                    // It is possible for the task to complete right after we snap the copy of
                    // the list.  If so, then fall through and return false without queuing the
                    // continuation.
                    if (m_continuationObject != s_taskCompletionSentinel)
                    {
                        // Before growing the list we remove possible null entries that are the
                        // result from RemoveContinuations()
                        //if (list.Count == list.Capacity)
                        //{
                        //    list.RemoveAll(l => l == null);
                        //}

                        if (addBeforeOthers)
                            list.Insert(0, tc);
                        else
                            list.Add(tc);

                        return true; // continuation successfully queued, so return true.
                    }
                }
            }

            //Diagnostics.Log.LogObject("can't, taskCompletionSentinel", this);

            // We didn't succeed in queuing the continuation, so return false.
            return false;
        }

        public bool IsFaulted =>
            (m_stateFlags & (int)TaskStateFlags.Faulted) != 0;
        internal bool IsExceptionObservedByParent => (m_stateFlags & (int)TaskStateFlags.ExceptionObservedByParent) != 0;

        internal bool TrySetException(object exceptionObject)
        {
            bool returnValue = false;
            EnsureContingentPropertiesInitialized();
            if (AtomicStateUpdate(
                (int)TaskStateFlags.CompletionReserved,
                (int)TaskStateFlags.CompletionReserved | (int)TaskStateFlags.RanToCompletion | (int)TaskStateFlags.Faulted | (int)TaskStateFlags.Canceled))
            {
                //AddException(exceptionObject);
                //Finish(false);
                returnValue = true;
            }

            return returnValue;
        }

        internal void ProcessChildCompletion(Task childTask)
        {
            ContingentProperties? props = Volatile.Read(ref m_contingentProperties);
            if (childTask.IsFaulted && !childTask.IsExceptionObservedByParent)
            {
                if (props!.m_exceptionalChildren == null)
                {
                    Interlocked.CompareExchange(ref props.m_exceptionalChildren, new List<Task>(), null);
                }

                List<Task>? tmp = props.m_exceptionalChildren;
                if (tmp != null)
                {
                    lock (tmp)
                    {
                        tmp.Add(childTask);
                    }
                }
            }

            if (Interlocked.Decrement(ref props!.m_completionCountdown) == 0)
            {
                FinishStageTwo();
            }
        }

        internal void Finish(bool userDelegateExecute)
        {
            if (m_contingentProperties == null)
            {
                FinishStageTwo();
            }
            else
            {
                FinishSlow(userDelegateExecute);
            }
        }
        internal bool MarkStarted()
        {
            return AtomicStateUpdate((int)TaskStateFlags.Started, (int)TaskStateFlags.Canceled | (int)TaskStateFlags.Started);
        }
        private void FinishSlow(bool userDelegateExecute)
        {
            if (!userDelegateExecute)
            {
                FinishStageTwo();
            }
            else
            {
                ContingentProperties props = m_contingentProperties!;
                if ((props.m_completionCountdown == 1) ||
                    Interlocked.Decrement(ref props.m_completionCountdown) == 0)
                {
                    FinishStageTwo();
                }
                else
                {
                    AtomicStateUpdate((int)TaskStateFlags.WaitingOnChildren, (int)TaskStateFlags.Faulted | (int)TaskStateFlags.Canceled | (int)TaskStateFlags.RanToCompletion);
                }
                List<Task>? exceptionalChildren = props.m_exceptionalChildren;
                if (exceptionalChildren != null)
                {
                    lock (exceptionalChildren)
                    {
                        exceptionalChildren.RemoveAll(t => t.IsExceptionObservedByParent);
                    }
                }
            }
        }


        private void FinishStageTwo()
        {
            ContingentProperties? cp = Volatile.Read(ref m_contingentProperties);
            int completionState;
            
            completionState = (int)TaskStateFlags.RanToCompletion;

            Interlocked.Exchange(ref m_stateFlags, m_stateFlags | completionState);

            cp = Volatile.Read(ref m_contingentProperties); // need to re-read after updating state
            if (cp != null)
            {
                cp.SetCompleted();
                cp.UnregisterCancellationCallback();
            }

            FinishStageThree();
        }

        internal void FinishStageThree()
        {
            m_action = null;

            ContingentProperties? cp = m_contingentProperties;
            if (cp != null)
            {
                //cp.m_capturedContext = null;

                NotifyParentIfPotentiallyAttachedTask();
            }
            FinishContinuations();
        }
        // Atomically OR-in newBits to m_stateFlags, while making sure that
        // no illegalBits are set.  Returns true on success, false on failure.
        internal bool AtomicStateUpdate(int newBits, int illegalBits)
        {
            int oldFlags = m_stateFlags;
            return
                (oldFlags & illegalBits) == 0 &&
                (Interlocked.CompareExchange(ref m_stateFlags, oldFlags | newBits, oldFlags) == oldFlags ||
                 AtomicStateUpdateSlow(newBits, illegalBits));
        }

        private bool AtomicStateUpdateSlow(int newBits, int illegalBits)
        {
            int flags = m_stateFlags;
            while (true)
            {
                if ((flags & illegalBits) != 0) return false;
                int oldFlags = Interlocked.CompareExchange(ref m_stateFlags, flags | newBits, flags);
                if (oldFlags == flags)
                {
                    return true;
                }
                flags = oldFlags;
            }
        }

        internal bool AtomicStateUpdate(int newBits, int illegalBits, ref int oldFlags)
        {
            int flags = oldFlags = m_stateFlags;
            while (true)
            {
                if ((flags & illegalBits) != 0) return false;
                oldFlags = Interlocked.CompareExchange(ref m_stateFlags, flags | newBits, flags);
                if (oldFlags == flags)
                {
                    return true;
                }
                flags = oldFlags;
            }
        }


        internal void AddNewChild()
        {
            ContingentProperties props = EnsureContingentPropertiesInitialized();
            // TODO: (perf) don't increment if nothing is there's only one thread 
            Interlocked.Increment(ref props.m_completionCountdown);
        }

        public void Dispose()
        {
            // TODO:
        }


        internal bool TrySetResult()
        {
            if (AtomicStateUpdate(
                (int)TaskStateFlags.CompletionReserved | (int)TaskStateFlags.RanToCompletion,
                (int)TaskStateFlags.CompletionReserved | (int)TaskStateFlags.RanToCompletion | (int)TaskStateFlags.Faulted | (int)TaskStateFlags.Canceled))
            {
                ContingentProperties? props = m_contingentProperties;
                if (props != null)
                {
                    NotifyParentIfPotentiallyAttachedTask();
                    props.SetCompleted();
                }

                FinishContinuations();

                return true;
            }

            return false;
        }


        public static Task<TResult> FromResult<TResult>(TResult result)
        {
            // TODO: default result task
            return new Task<TResult>(result);
        }

        internal static Task? InternalCurrent => t_currentTask;


        internal sealed class ContingentProperties
        {
            // TODO: ExecutionContext

            internal volatile ManualResetEvent? m_completionEvent;
            //internal volatile TaskExceptionHolder? m_exceptionsHolder;
            internal CancellationToken m_cancellationToken;
            //internal StrongBox<CancellationTokenRegistration>? m_cancellationRegistration;
            internal volatile int m_internalCancellationRequested;

            internal volatile int m_completionCountdown = 1;
            internal volatile List<Task>? m_exceptionalChildren;
            internal Task? m_parent;

            internal void SetCompleted()
            {
                ManualResetEvent? mres = m_completionEvent;
                if (mres != null) SetEvent(mres);
            }

            internal static void SetEvent(ManualResetEvent mres)
            {
                try
                {
                    mres.Set();
                }
                catch (ObjectDisposedException)
                {
                }
            }

            internal void UnregisterCancellationCallback()
            {
                /*if (m_cancellationRegistration != null)
                {
                    try { m_cancellationRegistration.Value.Dispose(); }
                    catch (ObjectDisposedException) { }
                    m_cancellationRegistration = null;
                }*/
            }
        }
    }

    [Flags]
    public enum TaskCreationOptions
    {
        None = 0x0,
        PreferFairness = 0x01,
        LongRunning = 0x02,
        AttachedToParent = 0x04,
        DenyChildAttach = 0x08,
        HideScheduler = 0x10,
        RunContinuationsAsynchronously = 0x40
    }

    [Flags]
    internal enum InternalTaskOptions
    {
        None = 0x0,
        InternalOptionsMask = 0x0000FF00,
        ContinuationTask = 0x0200,
        PromiseTask = 0x0400,
        LazyCancellation = 0x1000,
        QueuedByRuntime = 0x2000,
        DoNotDispose = 0x4000
    }

    [Flags]
    public enum TaskContinuationOptions
    {
        None = 0,
        PreferFairness = 0x01,
        LongRunning = 0x02,
        AttachedToParent = 0x04,
        DenyChildAttach = 0x08,
        HideScheduler = 0x10,
        LazyCancellation = 0x20,
        RunContinuationsAsynchronously = 0x40,
        NotOnRanToCompletion = 0x10000,
        NotOnFaulted = 0x20000,
        NotOnCanceled = 0x40000,
        OnlyOnRanToCompletion = NotOnFaulted | NotOnCanceled,
        OnlyOnFaulted = NotOnRanToCompletion | NotOnCanceled,
        OnlyOnCanceled = NotOnRanToCompletion | NotOnFaulted,
        ExecuteSynchronously = 0x80000
    }

}
