// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;

namespace System.Runtime.CompilerServices
{
    public struct AsyncTaskMethodBuilder<TResult>
    {
        private Task<TResult>? m_task;
        public static AsyncTaskMethodBuilder<TResult> Create() => default;
        public void Start<TStateMachine>(ref TStateMachine stateMachine) where TStateMachine : IAsyncStateMachine =>
            AsyncMethodBuilderCore.Start(ref stateMachine);

        // Legacy, I think. It should call AsyncMethodBuilderCore.SetStateMachine, but it's a no-op
        public void SetStateMachine(IAsyncStateMachine stateMachine)
        {
        }

        public void AwaitOnCompleted<TAwaiter, TStateMachine>(
            ref TAwaiter awaiter, ref TStateMachine stateMachine)
            where TAwaiter : INotifyCompletion
            where TStateMachine : IAsyncStateMachine =>
            AwaitOnCompleted(ref awaiter, ref stateMachine, ref m_task);

        internal static void AwaitOnCompleted<TAwaiter, TStateMachine>(
            ref TAwaiter awaiter, ref TStateMachine stateMachine, ref Task<TResult>? taskField)
            where TAwaiter : INotifyCompletion
            where TStateMachine : IAsyncStateMachine
        {
            awaiter.OnCompleted(GetStateMachineBox(ref stateMachine, ref taskField).MoveNextAction);
        }

        public void AwaitUnsafeOnCompleted<TAwaiter, TStateMachine>(
            ref TAwaiter awaiter, ref TStateMachine stateMachine)
            where TAwaiter : ICriticalNotifyCompletion
            where TStateMachine : IAsyncStateMachine =>
            AwaitUnsafeOnCompleted(ref awaiter, ref stateMachine, ref m_task);

        // TODO: taskField is nonnull, so you can elide the check
        internal static void AwaitUnsafeOnCompleted<TAwaiter, TStateMachine>(
            ref TAwaiter awaiter, ref TStateMachine stateMachine, ref Task<TResult>? taskField)
            where TAwaiter : ICriticalNotifyCompletion
            where TStateMachine : IAsyncStateMachine
        {
            IAsyncStateMachineBox box = GetStateMachineBox(ref stateMachine, ref taskField);
            AwaitUnsafeOnCompleted(ref awaiter, box);
        }
        internal static void AwaitUnsafeOnCompleted<TAwaiter>(
            ref TAwaiter awaiter, IAsyncStateMachineBox box)
            where TAwaiter : ICriticalNotifyCompletion
        {
            awaiter.UnsafeOnCompleted(box.MoveNextAction);
        }

        private static IAsyncStateMachineBox GetStateMachineBox<TStateMachine>(
            ref TStateMachine stateMachine,
            ref Task<TResult>? taskField)
            where TStateMachine : IAsyncStateMachine
        {
            // TODO: ExecutionContext
            if (taskField is AsyncStateMachineBox<TStateMachine> stronglyTypedBox)
            {
                return stronglyTypedBox;
            }

            AsyncStateMachineBox<TStateMachine> box = new AsyncStateMachineBox<TStateMachine>();
            taskField = box;
            box.StateMachine = stateMachine;

            return box;
        }

        private class AsyncStateMachineBox<TStateMachine> :
            Task<TResult>, IAsyncStateMachineBox
            where TStateMachine : IAsyncStateMachine
        {
            private Action? _moveNextAction;
            public TStateMachine? StateMachine;

            public Action MoveNextAction => _moveNextAction ??= new Action(MoveNext);

            public void MoveNext()
            {
                // TODO: check that it's not completed and that statemachine is nonnull
                // if the executionContext is not null, run it on the threadpool
                StateMachine.MoveNext();

                if (IsCompleted)
                {
                    StateMachine = default;
                }
            }
        }

        public Task<TResult> Task
        {
            get => m_task ??= new Task<TResult>();
        }
        

        public void SetResult(TResult result)
        {
            if (m_task is null)
            {
                m_task = Threading.Tasks.Task.FromResult(result);
            }
            else
            {
                SetExistingTaskResult(m_task, result);
            }
        }


        internal static void SetExistingTaskResult(Task<TResult> task, TResult? result)
        {
            // TODO: check for success
            task.TrySetResult(result);
        }

        public void SetException(Exception exception) => SetException(exception, ref m_task);

        internal static void SetException(Exception exception, ref Task<TResult>? taskField)
        {
            // TODO: check for exception not being null

            // Get the task, forcing initialization if it hasn't already been initialized.
            Task<TResult> task = (taskField ??= new Task<TResult>());

            // TODO: for cancellation I should call TrySetCanceled instead of TrySetException
            // TODO: check for success
            task.TrySetException(exception);
        }
    }
}