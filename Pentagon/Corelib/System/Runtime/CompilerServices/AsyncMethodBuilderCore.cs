// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System.Reflection;
using System.Threading;
using System.Threading.Tasks;

namespace System.Runtime.CompilerServices
{
    internal static class AsyncMethodBuilderCore
    {
        public static void Start<TStateMachine>(ref TStateMachine stateMachine) where TStateMachine : IAsyncStateMachine
        {
            // TODO: make sure statemachine is not null
            stateMachine.MoveNext();
            // TODO: wrap MoveNext in a try-finally and handle execution context switches
        }

        // Legacy function, it can just be a nop I think
        public static void SetStateMachine(IAsyncStateMachine stateMachine, Task? task)
        {
        }
    }
}