// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.


using System.Collections.Generic;
using System.Threading.Tasks;

namespace System.Runtime.CompilerServices
{
    public readonly struct TaskAwaiter : INotifyCompletion, ITaskAwaiter
    {
        private readonly Task m_task;
        internal TaskAwaiter(Task task)
        {
            m_task = task;
        }
        public bool IsCompleted => m_task.IsCompleted;

        public void OnCompleted(Action continuation)
        {
        }

        public void UnsafeOnCompleted(Action continuation)
        {
        }

        public void GetResult()
        {
        }


        internal static void OnCompletedInternal(Task task, Action continuation, bool continueOnCapturedContext, bool flowExecutionContext)
        {
            //ArgumentNullException.ThrowIfNull(continuation);

            task.SetContinuationForAwait(continuation, continueOnCapturedContext, flowExecutionContext);
        }
    }

    public readonly struct TaskAwaiter<TResult> : ICriticalNotifyCompletion, ITaskAwaiter
    {
        // WARNING: Unsafe.As is used to access TaskAwaiter<> as the non-generic TaskAwaiter.
        // Its layout must remain the same.

        private readonly Task<TResult> m_task;

        internal TaskAwaiter(Task<TResult> task)
        {
            m_task = task;
        }

        public bool IsCompleted => m_task.IsCompleted;

        public void OnCompleted(Action continuation)
        {
            TaskAwaiter.OnCompletedInternal(m_task, continuation, continueOnCapturedContext: true, flowExecutionContext: true);
        }

        public void UnsafeOnCompleted(Action continuation)
        {
            TaskAwaiter.OnCompletedInternal(m_task, continuation, continueOnCapturedContext: true, flowExecutionContext: false);
        }

        public TResult GetResult()
        {
            //TaskAwaiter.ValidateEnd(m_task);
            return m_task.ResultOnSuccess;
        }
    }

    internal interface ITaskAwaiter { }
    internal interface IConfiguredTaskAwaiter { }
    public readonly struct ConfiguredTaskAwaitable
    {
        /// <summary>The task being awaited.</summary>
        private readonly ConfiguredTaskAwaitable.ConfiguredTaskAwaiter m_configuredTaskAwaiter;

        /// <summary>Initializes the <see cref="ConfiguredTaskAwaitable"/>.</summary>
        /// <param name="task">The awaitable <see cref="System.Threading.Tasks.Task"/>.</param>
        /// <param name="continueOnCapturedContext">
        /// true to attempt to marshal the continuation back to the original context captured; otherwise, false.
        /// </param>
        internal ConfiguredTaskAwaitable(Task task, bool continueOnCapturedContext)
        {
            //Debug.Assert(task != null, "Constructing an awaitable requires a task to await.");
            m_configuredTaskAwaiter = new ConfiguredTaskAwaitable.ConfiguredTaskAwaiter(task, continueOnCapturedContext);
        }

        /// <summary>Gets an awaiter for this awaitable.</summary>
        /// <returns>The awaiter.</returns>
        public ConfiguredTaskAwaitable.ConfiguredTaskAwaiter GetAwaiter()
        {
            return m_configuredTaskAwaiter;
        }

        /// <summary>Provides an awaiter for a <see cref="ConfiguredTaskAwaitable"/>.</summary>
        /// <remarks>This type is intended for compiler use only.</remarks>
        public readonly struct ConfiguredTaskAwaiter : ICriticalNotifyCompletion, IConfiguredTaskAwaiter
        {
            // WARNING: Unsafe.As is used to access the generic ConfiguredTaskAwaiter as this.
            // Its layout must remain the same.

            /// <summary>The task being awaited.</summary>
            internal readonly Task m_task;
            /// <summary>Whether to attempt marshaling back to the original context.</summary>
            internal readonly bool m_continueOnCapturedContext;

            /// <summary>Initializes the <see cref="ConfiguredTaskAwaiter"/>.</summary>
            /// <param name="task">The <see cref="System.Threading.Tasks.Task"/> to await.</param>
            /// <param name="continueOnCapturedContext">
            /// true to attempt to marshal the continuation back to the original context captured
            /// when BeginAwait is called; otherwise, false.
            /// </param>
            internal ConfiguredTaskAwaiter(Task task, bool continueOnCapturedContext)
            {
                //Debug.Assert(task != null, "Constructing an awaiter requires a task to await.");
                m_task = task;
                m_continueOnCapturedContext = continueOnCapturedContext;
            }

            /// <summary>Gets whether the task being awaited is completed.</summary>
            /// <remarks>This property is intended for compiler user rather than use directly in code.</remarks>
            /// <exception cref="System.NullReferenceException">The awaiter was not properly initialized.</exception>
            public bool IsCompleted => m_task.IsCompleted;

            /// <summary>Schedules the continuation onto the <see cref="System.Threading.Tasks.Task"/> associated with this <see cref="TaskAwaiter"/>.</summary>
            /// <param name="continuation">The action to invoke when the await operation completes.</param>
            /// <exception cref="System.ArgumentNullException">The <paramref name="continuation"/> argument is null (Nothing in Visual Basic).</exception>
            /// <exception cref="System.NullReferenceException">The awaiter was not properly initialized.</exception>
            /// <remarks>This method is intended for compiler use rather than use directly in code.</remarks>
            public void OnCompleted(Action continuation)
            {
                TaskAwaiter.OnCompletedInternal(m_task, continuation, m_continueOnCapturedContext, flowExecutionContext: true);
            }

            /// <summary>Schedules the continuation onto the <see cref="System.Threading.Tasks.Task"/> associated with this <see cref="TaskAwaiter"/>.</summary>
            /// <param name="continuation">The action to invoke when the await operation completes.</param>
            /// <exception cref="System.ArgumentNullException">The <paramref name="continuation"/> argument is null (Nothing in Visual Basic).</exception>
            /// <exception cref="System.NullReferenceException">The awaiter was not properly initialized.</exception>
            /// <remarks>This method is intended for compiler use rather than use directly in code.</remarks>
            public void UnsafeOnCompleted(Action continuation)
            {
                TaskAwaiter.OnCompletedInternal(m_task, continuation, m_continueOnCapturedContext, flowExecutionContext: false);
            }

            /// <summary>Ends the await on the completed <see cref="System.Threading.Tasks.Task"/>.</summary>
            /// <exception cref="System.NullReferenceException">The awaiter was not properly initialized.</exception>
            /// <exception cref="System.Threading.Tasks.TaskCanceledException">The task was canceled.</exception>
            /// <exception cref="System.Exception">The task completed in a Faulted state.</exception>
            //[StackTraceHidden]
            public void GetResult()
            {
                //TaskAwaiter.ValidateEnd(m_task);
            }
        }
    }

    /// <summary>Provides an awaitable object that allows for configured awaits on <see cref="System.Threading.Tasks.Task{TResult}"/>.</summary>
    /// <remarks>This type is intended for compiler use only.</remarks>
    public readonly struct ConfiguredTaskAwaitable<TResult>
    {
        /// <summary>The underlying awaitable on whose logic this awaitable relies.</summary>
        private readonly ConfiguredTaskAwaitable<TResult>.ConfiguredTaskAwaiter m_configuredTaskAwaiter;

        /// <summary>Initializes the <see cref="ConfiguredTaskAwaitable{TResult}"/>.</summary>
        /// <param name="task">The awaitable <see cref="System.Threading.Tasks.Task{TResult}"/>.</param>
        /// <param name="continueOnCapturedContext">
        /// true to attempt to marshal the continuation back to the original context captured; otherwise, false.
        /// </param>
        internal ConfiguredTaskAwaitable(Task<TResult> task, bool continueOnCapturedContext)
        {
            m_configuredTaskAwaiter = new ConfiguredTaskAwaitable<TResult>.ConfiguredTaskAwaiter(task, continueOnCapturedContext);
        }

        /// <summary>Gets an awaiter for this awaitable.</summary>
        /// <returns>The awaiter.</returns>
        public ConfiguredTaskAwaitable<TResult>.ConfiguredTaskAwaiter GetAwaiter()
        {
            return m_configuredTaskAwaiter;
        }

        /// <summary>Provides an awaiter for a <see cref="ConfiguredTaskAwaitable{TResult}"/>.</summary>
        /// <remarks>This type is intended for compiler use only.</remarks>
        public readonly struct ConfiguredTaskAwaiter : ICriticalNotifyCompletion, IConfiguredTaskAwaiter
        {
            // WARNING: Unsafe.As is used to access this as the non-generic ConfiguredTaskAwaiter.
            // Its layout must remain the same.

            /// <summary>The task being awaited.</summary>
            private readonly Task<TResult> m_task;
            /// <summary>Whether to attempt marshaling back to the original context.</summary>
            private readonly bool m_continueOnCapturedContext;

            /// <summary>Initializes the <see cref="ConfiguredTaskAwaiter"/>.</summary>
            /// <param name="task">The awaitable <see cref="System.Threading.Tasks.Task{TResult}"/>.</param>
            /// <param name="continueOnCapturedContext">
            /// true to attempt to marshal the continuation back to the original context captured; otherwise, false.
            /// </param>
            internal ConfiguredTaskAwaiter(Task<TResult> task, bool continueOnCapturedContext)
            {
                //Debug.Assert(task != null, "Constructing an awaiter requires a task to await.");
                m_task = task;
                m_continueOnCapturedContext = continueOnCapturedContext;
            }

            /// <summary>Gets whether the task being awaited is completed.</summary>
            /// <remarks>This property is intended for compiler user rather than use directly in code.</remarks>
            /// <exception cref="System.NullReferenceException">The awaiter was not properly initialized.</exception>
            public bool IsCompleted => m_task.IsCompleted;

            /// <summary>Schedules the continuation onto the <see cref="System.Threading.Tasks.Task"/> associated with this <see cref="TaskAwaiter"/>.</summary>
            /// <param name="continuation">The action to invoke when the await operation completes.</param>
            /// <exception cref="System.ArgumentNullException">The <paramref name="continuation"/> argument is null (Nothing in Visual Basic).</exception>
            /// <exception cref="System.NullReferenceException">The awaiter was not properly initialized.</exception>
            /// <remarks>This method is intended for compiler use rather than use directly in code.</remarks>
            public void OnCompleted(Action continuation)
            {
                TaskAwaiter.OnCompletedInternal(m_task, continuation, m_continueOnCapturedContext, flowExecutionContext: true);
            }

            /// <summary>Schedules the continuation onto the <see cref="System.Threading.Tasks.Task"/> associated with this <see cref="TaskAwaiter"/>.</summary>
            /// <param name="continuation">The action to invoke when the await operation completes.</param>
            /// <exception cref="System.ArgumentNullException">The <paramref name="continuation"/> argument is null (Nothing in Visual Basic).</exception>
            /// <exception cref="System.NullReferenceException">The awaiter was not properly initialized.</exception>
            /// <remarks>This method is intended for compiler use rather than use directly in code.</remarks>
            public void UnsafeOnCompleted(Action continuation)
            {
                TaskAwaiter.OnCompletedInternal(m_task, continuation, m_continueOnCapturedContext, flowExecutionContext: false);
            }

            /// <summary>Ends the await on the completed <see cref="System.Threading.Tasks.Task{TResult}"/>.</summary>
            /// <returns>The result of the completed <see cref="System.Threading.Tasks.Task{TResult}"/>.</returns>
            /// <exception cref="System.NullReferenceException">The awaiter was not properly initialized.</exception>
            /// <exception cref="System.Threading.Tasks.TaskCanceledException">The task was canceled.</exception>
            /// <exception cref="System.Exception">The task completed in a Faulted state.</exception>
            /*[StackTraceHidden]*/
            public TResult GetResult()
            {
                // TODO: TaskAwaiter.ValidateEnd(m_task);
                return m_task.ResultOnSuccess;
            }
        }
    }
}