// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System.Collections.Generic;
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
        private /*volatile */ object? m_continuationObject;
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

        internal TaskCreationOptions Options => (TaskCreationOptions)(m_stateFlags & (int)TaskStateFlags.OptionsMask);
        public TaskCreationOptions CreationOptions => Options & (TaskCreationOptions)(~InternalTaskOptions.InternalOptionsMask);
        internal TaskScheduler? ExecutingTaskScheduler => m_taskScheduler;

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
            if ((creationOptions & ~(TaskCreationOptions.AttachedToParent | TaskCreationOptions.RunContinuationsAsynchronously)) != 0)
            {
                //ThrowHelper.ThrowArgumentOutOfRangeException(ExceptionArgument.creationOptions);
            }

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

        public Task(Action action)
            : this(action, null, null, default, TaskCreationOptions.None, InternalTaskOptions.None, null)
        {
        }

        public Task(Action action, CancellationToken cancellationToken)
            : this(action, null, null, cancellationToken, TaskCreationOptions.None, InternalTaskOptions.None, null)
        {
        }

        public Task(Action action, TaskCreationOptions creationOptions)
            : this(action, null, Task.InternalCurrentIfAttached(creationOptions), default, creationOptions, InternalTaskOptions.None, null)
        {
        }

        public Task(Action action, CancellationToken cancellationToken, TaskCreationOptions creationOptions)
            : this(action, null, Task.InternalCurrentIfAttached(creationOptions), cancellationToken, creationOptions, InternalTaskOptions.None, null)
        {
        }

        public Task(Action<object?> action, object? state)
            : this(action, state, null, default, TaskCreationOptions.None, InternalTaskOptions.None, null)
        {
        }
        public Task(Action<object?> action, object? state, CancellationToken cancellationToken)
            : this(action, state, null, cancellationToken, TaskCreationOptions.None, InternalTaskOptions.None, null)
        {
        }

        public Task(Action<object?> action, object? state, TaskCreationOptions creationOptions)
            : this(action, state, Task.InternalCurrentIfAttached(creationOptions), default, creationOptions, InternalTaskOptions.None, null)
        {
        }

        public Task(Action<object?> action, object? state, CancellationToken cancellationToken, TaskCreationOptions creationOptions)
            : this(action, state, Task.InternalCurrentIfAttached(creationOptions), cancellationToken, creationOptions, InternalTaskOptions.None, null)
        {
        }

        internal Task(Delegate action, object? state, Task? parent, CancellationToken cancellationToken,
            TaskCreationOptions creationOptions, InternalTaskOptions internalOptions, TaskScheduler? scheduler)
        {
            if (action == null)
            {
                //ThrowHelper.ThrowArgumentNullException(ExceptionArgument.action);
            }

            if (parent != null && (creationOptions & TaskCreationOptions.AttachedToParent) != 0)
            {
                EnsureContingentPropertiesInitializedUnsafe().m_parent = parent;
            }

            TaskConstructorCore(action, state, cancellationToken, creationOptions, internalOptions, scheduler);

            //CapturedContext = ExecutionContext.Capture();
        }


        internal static Task? InternalCurrentIfAttached(TaskCreationOptions creationOptions)
        {
            return (creationOptions & TaskCreationOptions.AttachedToParent) != 0 ? InternalCurrent : null;
        }
        internal ContingentProperties EnsureContingentPropertiesInitialized()
        {
            //return Volatile.Read(ref m_contingentProperties) ?? InitializeContingentProperties();
            return m_contingentProperties ?? InitializeContingentProperties();

            ContingentProperties InitializeContingentProperties()
            {
                //Interlocked.CompareExchange(ref m_contingentProperties, new ContingentProperties(), null);
                if (m_contingentProperties == null) m_contingentProperties = new ContingentProperties();
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
            //object? continuationObject = Interlocked.Exchange(ref m_continuationObject, s_taskCompletionSentinel);
            object? continuationObject = m_continuationObject;
            m_continuationObject = s_taskCompletionSentinel;
            if (continuationObject != null)
            {
                RunContinuations(continuationObject);
            }
        }


        private void RunContinuations(object continuationObject) // separated out of FinishContinuations to enable it to be inlined
        {
            bool canInlineContinuations = true;

            switch (continuationObject)
            {
                case IAsyncStateMachineBox stateMachineBox:
                    AwaitTaskContinuation.RunOrScheduleAction(stateMachineBox, canInlineContinuations);
                    return;

                case Action action:
                    AwaitTaskContinuation.RunOrScheduleAction(action, canInlineContinuations);
                    return;

                case TaskContinuation tc:
                    tc.Run(this, canInlineContinuations);
                    return;

                /*case ITaskCompletionAction completionAction:
                    RunOrQueueCompletionAction(completionAction, canInlineContinuations);
                    LogFinishCompletionNotification();
                    return;*/
            }

            List<object?> continuations = (List<object?>)continuationObject;

            //lock (continuations) { }
            int continuationCount = continuations.Count;

            if (canInlineContinuations)
            {
                bool forceContinuationsAsync = false;
                for (int i = 0; i < continuationCount; i++)
                {
                    object? currentContinuation = continuations[i];
                    if (currentContinuation == null)
                    {
                        // The continuation was unregistered and null'd out, so just skip it.
                        continue;
                    }
                    // TODO:
                }
            }

            for (int i = 0; i < continuationCount; i++)
            {
                object? currentContinuation = continuations[i];
                if (currentContinuation == null)
                {
                    continue;
                }
                continuations[i] = null;

                switch (currentContinuation)
                {
                    case IAsyncStateMachineBox stateMachineBox:
                        AwaitTaskContinuation.RunOrScheduleAction(stateMachineBox, canInlineContinuations);
                        break;

                    case Action action:
                        AwaitTaskContinuation.RunOrScheduleAction(action, canInlineContinuations);
                        break;

                    case TaskContinuation tc:
                        tc.Run(this, canInlineContinuations);
                        break;

                    /*default:
                        Debug.Assert(currentContinuation is ITaskCompletionAction);
                        RunOrQueueCompletionAction((ITaskCompletionAction)currentContinuation, canInlineContinuations);
                        break;*/
                }
            }
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

            //if (!IsCancellationRequested & !IsCanceled)
            //{
                ExecuteWithThreadLocal(ref t_currentTask);
            //}
            //else
            //{
            //    ExecuteEntryCancellationRequestedOrCanceled();
            //}

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

                // Execute the task body
                try
                {
                    /*ExecutionContext? ec = CapturedContext;
                    if (ec == null)
                    {*/
                        InnerInvoke();
                    //}
                    /*else
                    {
                        // Invoke it under the captured ExecutionContext
                        if (threadPoolThread is null)
                        {
                            ExecutionContext.RunInternal(ec, s_ecCallback, this);
                        }
                        else
                        {
                            ExecutionContext.RunFromThreadPoolDispatchLoop(threadPoolThread, ec, s_ecCallback, this);
                        }
                    }*/
                }
                catch (Exception exn)
                {
                    //HandleException(exn);
                }
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
            //ContingentProperties? props = Volatile.Read(ref m_contingentProperties);
            ContingentProperties? props = m_contingentProperties;
            if (childTask.IsFaulted && !childTask.IsExceptionObservedByParent)
            {
                if (props!.m_exceptionalChildren == null)
                {
                    //Interlocked.CompareExchange(ref props.m_exceptionalChildren, new List<Task>(), null);
                    if (props.m_exceptionalChildren == null) props.m_exceptionalChildren = new List<Task>();
                }

                List<Task>? tmp = props.m_exceptionalChildren;
                if (tmp != null)
                {
                    //lock (tmp)
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
                    //lock (exceptionalChildren)
                    {
                        //exceptionalChildren.RemoveAll(t => t.IsExceptionObservedByParent);
                    }
                }
            }
        }


        private void FinishStageTwo()
        {
            //ContingentProperties? cp = Volatile.Read(ref m_contingentProperties);
            ContingentProperties? cp = m_contingentProperties;
            /*if (cp != null)
            {
                AddExceptionsFromChildren(cp);
            }*/

            int completionState;
            /*if (ExceptionRecorded)
            {
                completionState = (int)TaskStateFlags.Faulted;
            }
            else if (IsCancellationRequested && IsCancellationAcknowledged)
            {
                completionState = (int)TaskStateFlags.Canceled;
            }
            else*/
            {
                completionState = (int)TaskStateFlags.RanToCompletion;
            }

            Interlocked.Exchange(ref m_stateFlags, m_stateFlags | completionState);

            //cp = Volatile.Read(ref m_contingentProperties); // need to re-read after updating state
            cp = m_contingentProperties;
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
        internal bool AtomicStateUpdate(int newBits, int illegalBits)
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

        internal void TaskConstructorCore(Delegate? action, object? state, CancellationToken cancellationToken,
            TaskCreationOptions creationOptions, InternalTaskOptions internalOptions, TaskScheduler? scheduler)
        {
            m_action = action;
            m_stateObject = state;

            // TODO: check option validity
            int tmpFlags = (int)creationOptions | (int)internalOptions;
            m_stateFlags = m_action == null || (internalOptions & InternalTaskOptions.ContinuationTask) != 0 ?
                tmpFlags | (int)TaskStateFlags.WaitingForActivation :
                tmpFlags;

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

            if (cancellationToken.CanBeCanceled)
            {
                //AssignCancellationToken(cancellationToken, null, null);
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
            m_stateFlags = (int)TaskStateFlags.CompletionReserved | (int)TaskStateFlags.RanToCompletion;
            return true;
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

            internal /*volatile*/ ManualResetEvent? m_completionEvent;
            //internal volatile TaskExceptionHolder? m_exceptionsHolder;
            internal CancellationToken m_cancellationToken;
            //internal StrongBox<CancellationTokenRegistration>? m_cancellationRegistration;
            internal /*volatile */ int m_internalCancellationRequested;

            internal /*volatile*/ int m_completionCountdown = 1;
            internal /*volatile*/ List<Task>? m_exceptionalChildren;
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
