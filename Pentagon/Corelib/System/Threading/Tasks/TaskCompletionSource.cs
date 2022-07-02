// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System.Collections.Generic;

namespace System.Threading.Tasks
{
    public class TaskCompletionSource
    {
        private readonly Task _task;
        public TaskCompletionSource() => _task = new Task();


        public TaskCompletionSource(TaskCreationOptions creationOptions) :
            this(null, creationOptions)
        {
        }

        public TaskCompletionSource(object? state) :
            this(state, TaskCreationOptions.None)
        {
        }

        public TaskCompletionSource(object? state, TaskCreationOptions creationOptions) =>
            _task = new Task(state, creationOptions, promiseStyle: true);
        public Task Task => _task;

        public void SetResult()
        {
            if (!TrySetResult())
            {
                // TODO: EH
            }
        }

        public bool TrySetResult()
        {
            bool rval = _task.TrySetResult();
            if (!rval)
            {
                // TODO: wait
            }
            return rval;
        }
    }
}