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
            OnCompletedInternal(m_task, continuation, continueOnCapturedContext: true, flowExecutionContext: true);
        }

        public void UnsafeOnCompleted(Action continuation)
        {
            OnCompletedInternal(m_task, continuation, continueOnCapturedContext: true, flowExecutionContext: false);
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
}