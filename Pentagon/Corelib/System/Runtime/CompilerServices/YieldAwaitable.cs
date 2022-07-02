// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
using System.Threading;
using System.Threading.Tasks;

namespace System.Threading
{
    public delegate void WaitCallback(object? state);
};

namespace System.Runtime.CompilerServices
{
    // TODO: this should not be here here
    internal interface IStateMachineBoxAwareAwaiter
    {
        void AwaitUnsafeOnCompleted(IAsyncStateMachineBox box);
    }
    public readonly struct YieldAwaitable
    {
        public static Action? StupidContinuation;

        public YieldAwaiter GetAwaiter() { return default; }
        public readonly struct YieldAwaiter : ICriticalNotifyCompletion, IStateMachineBoxAwareAwaiter
        {
            public bool IsCompleted => false; 
            public void OnCompleted(Action continuation)
            {
                QueueContinuation(continuation, flowContext: true);
            }

            public void UnsafeOnCompleted(Action continuation)
            {
                QueueContinuation(continuation, flowContext: false);
            }

            private static void QueueContinuation(Action continuation, bool flowContext)
            {
                //ArgumentNullException.ThrowIfNull(continuation);
                //ThreadPool.QueueUserWorkItem(s_waitCallbackRunAction, continuation);
                StupidContinuation = continuation;
            }

            void IStateMachineBoxAwareAwaiter.AwaitUnsafeOnCompleted(IAsyncStateMachineBox box)
            {
                 //ThreadPool.UnsafeQueueUserWorkItemInternal(box, preferLocal: false);
            }


            private static readonly WaitCallback s_waitCallbackRunAction = RunAction;
            private static void RunAction(object? state) { ((Action)state!)(); }

            public void GetResult() { }
        }
    }
}