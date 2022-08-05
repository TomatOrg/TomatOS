// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;

namespace System.Runtime.CompilerServices
{
    // This subtly differs from AsyncTaskMethodBuilder<VoidTaskResult>
    // as SetResult takes no parameters
    // and there is an optimization to avoid calling Task.FromResult()
    public struct AsyncTaskMethodBuilder
    {
        private Task<VoidTaskResult>? m_task;
        public static AsyncTaskMethodBuilder Create() => default;
        public void Start<TStateMachine>(ref TStateMachine stateMachine) where TStateMachine : IAsyncStateMachine =>
            AsyncMethodBuilderCore.Start(ref stateMachine);

        // Legacy, I think. It should call AsyncMethodBuilderCore.SetStateMachine, but it's a no-op
        public void SetStateMachine(IAsyncStateMachine stateMachine)
        {
        }

        public void SetResult()
        {
            if (m_task is null)
            {
                m_task = Task.s_cachedCompleted;
            }
            else
            {
                AsyncTaskMethodBuilder<VoidTaskResult>.SetExistingTaskResult(m_task, default!);
            }
        }
        public void AwaitOnCompleted<TAwaiter, TStateMachine>(
            ref TAwaiter awaiter, ref TStateMachine stateMachine)
            where TAwaiter : INotifyCompletion
            where TStateMachine : IAsyncStateMachine =>
            AsyncTaskMethodBuilder<VoidTaskResult>.AwaitOnCompleted(ref awaiter, ref stateMachine, ref m_task);

        public void AwaitUnsafeOnCompleted<TAwaiter, TStateMachine>(
            ref TAwaiter awaiter, ref TStateMachine stateMachine)
            where TAwaiter : ICriticalNotifyCompletion
            where TStateMachine : IAsyncStateMachine =>
            AsyncTaskMethodBuilder<VoidTaskResult>.AwaitUnsafeOnCompleted(ref awaiter, ref stateMachine, ref m_task);

        public Task Task
        {
            get => m_task ??= new Task<VoidTaskResult>();
        }

        public void SetException(Exception exception) =>
            AsyncTaskMethodBuilder<VoidTaskResult>.SetException(exception, ref m_task);
    }
}