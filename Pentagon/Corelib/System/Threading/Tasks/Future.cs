// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System.Collections.Generic;
using System.Runtime.CompilerServices;

namespace System.Threading.Tasks
{
    // lol
    public class TaskScheduler
    {
        internal bool TryRunInline(Task task, bool taskWasPreviouslyQueued)
        {
            TaskScheduler? ets = task.ExecutingTaskScheduler;

            if (ets != this && ets != null) return ets.TryRunInline(task, taskWasPreviouslyQueued);
            bool inlined = TryExecuteTaskInline(task, taskWasPreviouslyQueued);

            return inlined;
        }
        protected bool TryExecuteTaskInline(Task task, bool taskWasPreviouslyQueued)
        {
            return TryExecuteTask(task);
        }
        protected bool TryExecuteTask(Task task)
        {
            return task.ExecuteEntry();
        }
    }

    // Task<TResult> is here and not in Task.cs. WHYYY?????
    public class Task<TResult> : Task
    {
        internal TResult? m_result;
        // Construct a promise-style task without any options.
        internal Task()
        {
        }

        // Construct a promise-style task with state and options.
        internal Task(object? state, TaskCreationOptions options) :
            base(state, options, promiseStyle: true)
        {
        }

        internal Task(TResult result) :
            base(false, TaskCreationOptions.None, default)
        {
            m_result = result;
        }

        internal Task(bool canceled, TResult? result, TaskCreationOptions creationOptions, CancellationToken ct)
            : base(canceled, creationOptions, ct)
        {
            if (!canceled)
            {
                m_result = result;
            }
        }

        public Task(Delegate function)
            : this(function, null, null, default,
                TaskCreationOptions.None, InternalTaskOptions.None, null)
        {
        }

        public Task(Delegate function, CancellationToken cancellationToken)
            : this(function, null, null, cancellationToken,
                TaskCreationOptions.None, InternalTaskOptions.None, null)
        {
        }

        public Task(Delegate function, TaskCreationOptions creationOptions)
            : this(function, null, Task.InternalCurrentIfAttached(creationOptions), default, creationOptions, InternalTaskOptions.None, null)
        {
        }

        public Task(Delegate function, CancellationToken cancellationToken, TaskCreationOptions creationOptions)
            : this(function, null, Task.InternalCurrentIfAttached(creationOptions), cancellationToken, creationOptions, InternalTaskOptions.None, null)
        {
        }

        public Task(Delegate function, object state)
            : this(function, state, null, default,
                TaskCreationOptions.None, InternalTaskOptions.None, null)
        {
        }

        public Task(Delegate function, object state, CancellationToken cancellationToken)
            : this(function, state, null, cancellationToken,
                    TaskCreationOptions.None, InternalTaskOptions.None, null)
        {
        }

        public Task(Delegate function, object state, TaskCreationOptions creationOptions)
            : this(function, state, Task.InternalCurrentIfAttached(creationOptions), default,
                    creationOptions, InternalTaskOptions.None, null)
        {
        }

        public Task(Delegate function, object state, CancellationToken cancellationToken, TaskCreationOptions creationOptions)
            : this(function, state, Task.InternalCurrentIfAttached(creationOptions), cancellationToken,
                    creationOptions, InternalTaskOptions.None, null)
        {
        }

        internal Task(Func<TResult> valueSelector, Task? parent, CancellationToken cancellationToken,
            TaskCreationOptions creationOptions, InternalTaskOptions internalOptions, TaskScheduler? scheduler) :
            base(valueSelector, null, parent, cancellationToken, creationOptions, internalOptions, scheduler)
        {
        }

        internal Task(Delegate valueSelector, object state, Task parent, CancellationToken cancellationToken,
            TaskCreationOptions creationOptions, InternalTaskOptions internalOptions, TaskScheduler? scheduler) :
            base(valueSelector, state, parent, cancellationToken, creationOptions, internalOptions, scheduler)
        {
        }


        internal void MarkExceptionsAsHandled()
        {
            // TODO:
        }


        internal bool TrySetResult(TResult? result)
        {
            bool returnValue = false;
            if (AtomicStateUpdate((int)TaskStateFlags.CompletionReserved,
                    (int)TaskStateFlags.CompletionReserved | (int)TaskStateFlags.RanToCompletion | (int)TaskStateFlags.Faulted | (int)TaskStateFlags.Canceled))
            {
                m_result = result;
                Interlocked.Exchange(ref m_stateFlags, m_stateFlags | (int)TaskStateFlags.RanToCompletion);
                ContingentProperties? props = m_contingentProperties;
                if (props != null)
                {
                    NotifyParentIfPotentiallyAttachedTask();
                    props.SetCompleted();
                }
                FinishContinuations();
                returnValue = true;
            }

            return returnValue;
        }

        internal void DangerousSetResult(TResult result)
        {
            if (m_contingentProperties?.m_parent != null)
            {
                TrySetResult(result);
                // TODO: check for success
            }
            else
            {
                m_result = result;
                m_stateFlags |= (int)TaskStateFlags.RanToCompletion;
            }
        }

        public TResult Result => m_result!;


        internal TResult GetResultCore(bool waitCompletionNotification)
        {
            // TODO: wait
            //if (!IsCompleted) InternalWait(Timeout.Infinite, default); 
            //if (!IsCompletedSuccessfully) ThrowIfExceptional(includeTaskCanceledExceptions: true);
            return m_result!;
        }

        public new TaskAwaiter<TResult> GetAwaiter()
        {
            return new TaskAwaiter<TResult>(this);
        }
        

        /// <summary>Gets a <see cref="Task{TResult}"/> that will complete when this <see cref="Task{TResult}"/> completes or when the specified <see cref="CancellationToken"/> has cancellation requested.</summary>
        /// <param name="cancellationToken">The <see cref="CancellationToken"/> to monitor for a cancellation request.</param>
        /// <returns>The <see cref="Task{TResult}"/> representing the asynchronous wait.  It may or may not be the same instance as the current instance.</returns>
        public new Task<TResult> WaitAsync(CancellationToken cancellationToken) =>
            WaitAsync(Timeout.UnsignedInfinite, cancellationToken);

        /// <summary>Gets a <see cref="Task{TResult}"/> that will complete when this <see cref="Task{TResult}"/> completes or when the specified timeout expires.</summary>
        /// <param name="timeout">The timeout after which the <see cref="Task"/> should be faulted with a <see cref="TimeoutException"/> if it hasn't otherwise completed.</param>
        /// <returns>The <see cref="Task{TResult}"/> representing the asynchronous wait.  It may or may not be the same instance as the current instance.</returns>
        public new Task<TResult> WaitAsync(TimeSpan timeout) =>
            WaitAsync(timeout, default);

        /// <summary>Gets a <see cref="Task{TResult}"/> that will complete when this <see cref="Task{TResult}"/> completes, when the specified timeout expires, or when the specified <see cref="CancellationToken"/> has cancellation requested.</summary>
        /// <param name="timeout">The timeout after which the <see cref="Task"/> should be faulted with a <see cref="TimeoutException"/> if it hasn't otherwise completed.</param>
        /// <param name="cancellationToken">The <see cref="CancellationToken"/> to monitor for a cancellation request.</param>
        /// <returns>The <see cref="Task{TResult}"/> representing the asynchronous wait.  It may or may not be the same instance as the current instance.</returns>
        public new Task<TResult> WaitAsync(TimeSpan timeout, CancellationToken cancellationToken) =>
            WaitAsync(timeout, cancellationToken);

        private Task<TResult> WaitAsync(uint millisecondsTimeout, CancellationToken cancellationToken)
        {
            if (IsCompleted || (!cancellationToken.CanBeCanceled && millisecondsTimeout == Timeout.UnsignedInfinite))
            {
                return this;
            }
            // TODO:
            return this;
        }

        internal TResult ResultOnSuccess
        {
            get
            {
                return m_result!;
            }
        }
    }
}