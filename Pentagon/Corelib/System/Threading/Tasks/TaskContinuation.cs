// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System.Diagnostics;
using System.Runtime.CompilerServices;
using static System.Threading.ExecutionContext;

namespace System.Threading.Tasks
{
    // Task type used to implement: Task ContinueWith(Action<Task,...>)
    internal sealed class ContinuationTaskFromTask : Task
    {
        private Task? m_antecedent;

        public ContinuationTaskFromTask(
            Task antecedent, Delegate action, object? state, TaskCreationOptions creationOptions, InternalTaskOptions internalOptions) :
            base(action, state, Task.InternalCurrentIfAttached(creationOptions), default, creationOptions, internalOptions, null)
        {
            Debug.Assert(action is Action<Task> || action is Action<Task, object?>,
                "Invalid delegate type in ContinuationTaskFromTask");
            m_antecedent = antecedent;
        }

        /// <summary>
        /// Evaluates the value selector of the Task which is passed in as an object and stores the result.
        /// </summary>
        internal override void InnerInvoke()
        {
            // Get and null out the antecedent.  This is crucial to avoid a memory
            // leak with long chains of continuations.
            Task? antecedent = m_antecedent;
            Debug.Assert(antecedent != null,
                "No antecedent was set for the ContinuationTaskFromTask.");
            m_antecedent = null;


            // Invoke the delegate
            Debug.Assert(m_action != null);
            if (m_action is Action<Task> action)
            {
                action(antecedent);
                return;
            }

            if (m_action is Action<Task, object?> actionWithState)
            {
                actionWithState(antecedent, m_stateObject);
                return;
            }
            Debug.Fail("Invalid m_action in ContinuationTaskFromTask");
        }
    }

    // Task type used to implement: Task<TResult> ContinueWith(Func<Task,...>)
    internal sealed class ContinuationResultTaskFromTask<TResult> : Task<TResult>
    {
        private Task? m_antecedent;

        public ContinuationResultTaskFromTask(
            Task antecedent, Delegate function, object? state, TaskCreationOptions creationOptions, InternalTaskOptions internalOptions) :
            base(function, state, Task.InternalCurrentIfAttached(creationOptions), default, creationOptions, internalOptions, null)
        {
            Debug.Assert(function is Func<Task, TResult> || function is Func<Task, object?, TResult>,
                "Invalid delegate type in ContinuationResultTaskFromTask");
            m_antecedent = antecedent;
        }

        /// <summary>
        /// Evaluates the value selector of the Task which is passed in as an object and stores the result.
        /// </summary>
        internal override void InnerInvoke()
        {
            // Get and null out the antecedent.  This is crucial to avoid a memory
            // leak with long chains of continuations.
            Task? antecedent = m_antecedent;
            Debug.Assert(antecedent != null,
                "No antecedent was set for the ContinuationResultTaskFromTask.");
            m_antecedent = null;


            // Invoke the delegate
            Debug.Assert(m_action != null);
            if (m_action is Func<Task, TResult> func)
            {
                m_result = func(antecedent);
                return;
            }

            if (m_action is Func<Task, object?, TResult> funcWithState)
            {
                m_result = funcWithState(antecedent, m_stateObject);
                return;
            }
            Debug.Fail("Invalid m_action in ContinuationResultTaskFromTask");
        }
    }

    // Task type used to implement: Task ContinueWith(Action<Task<TAntecedentResult>,...>)
    internal sealed class ContinuationTaskFromResultTask<TAntecedentResult> : Task
    {
        private Task<TAntecedentResult>? m_antecedent;

        public ContinuationTaskFromResultTask(
            Task<TAntecedentResult> antecedent, Delegate action, object? state, TaskCreationOptions creationOptions, InternalTaskOptions internalOptions) :
            base(action, state, Task.InternalCurrentIfAttached(creationOptions), default, creationOptions, internalOptions, null)
        {
            Debug.Assert(action is Action<Task<TAntecedentResult>> || action is Action<Task<TAntecedentResult>, object?>,
                "Invalid delegate type in ContinuationTaskFromResultTask");
            m_antecedent = antecedent;
        }

        /// <summary>
        /// Evaluates the value selector of the Task which is passed in as an object and stores the result.
        /// </summary>
        internal override void InnerInvoke()
        {
            // Get and null out the antecedent.  This is crucial to avoid a memory
            // leak with long chains of continuations.
            Task<TAntecedentResult>? antecedent = m_antecedent;
            Debug.Assert(antecedent != null,
                "No antecedent was set for the ContinuationTaskFromResultTask.");
            m_antecedent = null;

            // Notify the debugger we're completing an asynchronous wait on a task

            // Invoke the delegate
            Debug.Assert(m_action != null);
            if (m_action is Action<Task<TAntecedentResult>> action)
            {
                action(antecedent);
                return;
            }

            if (m_action is Action<Task<TAntecedentResult>, object?> actionWithState)
            {
                actionWithState(antecedent, m_stateObject);
                return;
            }
            Debug.Fail("Invalid m_action in ContinuationTaskFromResultTask");
        }
    }

    // Task type used to implement: Task<TResult> ContinueWith(Func<Task<TAntecedentResult>,...>)
    internal sealed class ContinuationResultTaskFromResultTask<TAntecedentResult, TResult> : Task<TResult>
    {
        private Task<TAntecedentResult>? m_antecedent;

        public ContinuationResultTaskFromResultTask(
            Task<TAntecedentResult> antecedent, Delegate function, object? state, TaskCreationOptions creationOptions, InternalTaskOptions internalOptions) :
            base(function, state, Task.InternalCurrentIfAttached(creationOptions), default, creationOptions, internalOptions, null)
        {
            Debug.Assert(function is Func<Task<TAntecedentResult>, TResult> || function is Func<Task<TAntecedentResult>, object?, TResult>,
                "Invalid delegate type in ContinuationResultTaskFromResultTask");
            m_antecedent = antecedent;
        }

        /// <summary>
        /// Evaluates the value selector of the Task which is passed in as an object and stores the result.
        /// </summary>
        internal override void InnerInvoke()
        {
            // Get and null out the antecedent.  This is crucial to avoid a memory
            // leak with long chains of continuations.
            Task<TAntecedentResult>? antecedent = m_antecedent;
            Debug.Assert(antecedent != null,
                "No antecedent was set for the ContinuationResultTaskFromResultTask.");
            m_antecedent = null;

            // Notify the debugger we're completing an asynchronous wait on a task

            // Invoke the delegate
            Debug.Assert(m_action != null);
            if (m_action is Func<Task<TAntecedentResult>, TResult> func)
            {
                m_result = func(antecedent);
                return;
            }

            if (m_action is Func<Task<TAntecedentResult>, object?, TResult> funcWithState)
            {
                m_result = funcWithState(antecedent, m_stateObject);
                return;
            }
            Debug.Fail("Invalid m_action in ContinuationResultTaskFromResultTask");
        }
    }

    // For performance reasons, we don't just have a single way of representing
    // a continuation object.  Rather, we have a hierarchy of types:
    // - TaskContinuation: abstract base that provides a virtual Run method
    //     - StandardTaskContinuation: wraps a task,options,and scheduler, and overrides Run to process the task with that configuration
    //     - AwaitTaskContinuation: base for continuations created through TaskAwaiter; targets default scheduler by default
    //         - TaskSchedulerAwaitTaskContinuation: awaiting with a non-default TaskScheduler
    //         - SynchronizationContextAwaitTaskContinuation: awaiting with a "current" sync ctx

    /// <summary>Represents a continuation.</summary>
    internal abstract class TaskContinuation
    {
        internal abstract void Run(Task completedTask, bool canInlineContinuationTask);

        protected static void InlineIfPossibleOrElseQueue(Task task, bool needsProtection)
        {
            Debug.Assert(task != null);
            Debug.Assert(task.m_taskScheduler != null);

            // Set the TaskStateFlags.Started flag.  This only needs to be done
            // if the task may be canceled or if someone else has a reference to it
            // that may try to execute it.
            if (needsProtection)
            {
                if (!task.MarkStarted())
                    return; // task has been previously started or canceled.  Stop processing.
            }
            else
            {
                task.m_stateFlags |= (int)Task.TaskStateFlags.Started;
            }

            try
            {
                if (!task.m_taskScheduler.TryRunInline(task, taskWasPreviouslyQueued: false))
                {
                    Debug.Fail("we shouldn't get here");
                    // task.m_taskScheduler.InternalQueueTask(task);
                }
            }
            catch (Exception e)
            {

            }
        }

    }

    /// <summary>Provides the standard implementation of a task continuation.</summary>
    internal sealed class ContinueWithTaskContinuation : TaskContinuation
    {
        /// <summary>The unstarted continuation task.</summary>
        internal Task? m_task;
        /// <summary>The options to use with the continuation task.</summary>
        internal readonly TaskContinuationOptions m_options;
        /// <summary>The task scheduler with which to run the continuation task.</summary>
        private readonly TaskScheduler m_taskScheduler;


        internal ContinueWithTaskContinuation(Task task, TaskContinuationOptions options, TaskScheduler scheduler)
        {
            Debug.Assert(task != null, "TaskContinuation ctor: task is null");
            Debug.Assert(scheduler != null, "TaskContinuation ctor: scheduler is null");
            m_task = task;
            m_options = options;

        }

        internal override void Run(Task completedTask, bool canInlineContinuationTask)
        {
            Debug.Assert(completedTask != null);
            Debug.Assert(completedTask.IsCompleted, "ContinuationTask.Run(): completedTask not completed");

            Task? continuationTask = m_task;
            Debug.Assert(continuationTask != null);
            m_task = null;

            // Check if the completion status of the task works with the desired
            // activation criteria of the TaskContinuationOptions.
            TaskContinuationOptions options = m_options;
            bool isRightKind =
                completedTask.IsCompletedSuccessfully ?
                    (options & TaskContinuationOptions.NotOnRanToCompletion) == 0 :
                    (completedTask.IsCanceled ?
                        (options & TaskContinuationOptions.NotOnCanceled) == 0 :
                        (options & TaskContinuationOptions.NotOnFaulted) == 0);

            // If the completion status is allowed, run the continuation.
            if (isRightKind)
            {
                // If the task was cancel before running (e.g a ContinueWhenAll with a cancelled caancelation token)
                // we will still flow it to ScheduleAndStart() were it will check the status before running
                // We check here to avoid faulty logs that contain a join event to an operation that was already set as completed.

                continuationTask.m_taskScheduler = m_taskScheduler;

                // Either run directly or just queue it up for execution, depending
                // on whether synchronous or asynchronous execution is wanted.

                {
                    InlineIfPossibleOrElseQueue(continuationTask, needsProtection: true);
                }

            }
            else
            {
                //Diagnostics.Log.LogString("TODO: ");
            }
        }

    }

    /// <summary>Task continuation for awaiting with a task scheduler.</summary>
    internal sealed class TaskSchedulerAwaitTaskContinuation : AwaitTaskContinuation
    {
        private readonly TaskScheduler m_scheduler;

        internal TaskSchedulerAwaitTaskContinuation(
            TaskScheduler scheduler, Action action, bool flowExecutionContext) :
            base(action, flowExecutionContext)
        {
            Debug.Assert(scheduler != null);
            m_scheduler = scheduler;
        }

        internal sealed override void Run(Task ignored, bool canInlineContinuationTask)
        {

            base.Run(ignored, canInlineContinuationTask);

        }
    }

    internal class AwaitTaskContinuation : TaskContinuation, IThreadPoolWorkItem
    {
        private readonly ExecutionContext? m_capturedContext;
        protected readonly Action m_action;

        protected int m_continuationId;
        internal AwaitTaskContinuation(Action action, bool flowExecutionContext)
        {
            Debug.Assert(action != null);
            m_action = action;

        }

        protected Task CreateTask(Action<object?> action, object? state, TaskScheduler scheduler)
        {
            Debug.Assert(action != null);
            Debug.Assert(scheduler != null);

            return new Task(
                action, state, null, default,
                TaskCreationOptions.None, InternalTaskOptions.QueuedByRuntime, scheduler)
            {
            };
        }

        /// <summary>Inlines or schedules the continuation onto the default scheduler.</summary>
        /// <param name="task">The antecedent task, which is ignored.</param>
        /// <param name="canInlineContinuationTask">true if inlining is permitted; otherwise, false.</param>
        internal override void Run(Task task, bool canInlineContinuationTask)
        {
            RunCallback(GetInvokeActionCallback(), m_action, ref Task.t_currentTask); // any exceptions from m_action will be handled by s_callbackRunAction

        }

        internal static bool IsValidLocationForInlining
        {
            get
            {
                return true;
            }
        }

        void IThreadPoolWorkItem.Execute()
        {

            m_action();
        }

        private static readonly ContextCallback s_invokeContextCallback = static (state) =>
        {
            Debug.Assert(state is Action);
            ((Action)state)();
        };
        private static readonly Action<Action> s_invokeAction = (action) => action();

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        protected static ContextCallback GetInvokeActionCallback() => s_invokeContextCallback;


        protected void RunCallback(ContextCallback callback, object? state, ref Task? currentTask)
        {
            Debug.Assert(callback != null);
            Debug.Assert(currentTask == Task.t_currentTask);

            Task? prevCurrentTask = currentTask;
            {
                if (prevCurrentTask != null) currentTask = null;

                callback(state);
            }

            {
                // Restore the current task information
                if (prevCurrentTask != null) currentTask = prevCurrentTask;
            }
        }

        internal static void RunOrScheduleAction(Action action, bool allowInlining)
        {
            ref Task? currentTask = ref Task.t_currentTask;
            Task? prevCurrentTask = currentTask;



            // Otherwise, run it, making sure that t_currentTask is null'd out appropriately during the execution
            {
                if (prevCurrentTask != null) currentTask = null;
                action();
            }
            {
                if (prevCurrentTask != null) currentTask = prevCurrentTask;
            }
        }

        /// <summary>Invokes or schedules the action to be executed.</summary>
        /// <param name="box">The <see cref="IAsyncStateMachineBox"/> that needs to be invoked or queued.</param>
        /// <param name="allowInlining">
        /// true to allow inlining, or false to force the box's action to run asynchronously.
        /// </param>
        internal static void RunOrScheduleAction(IAsyncStateMachineBox box, bool allowInlining)
        {
            // Same logic as in the RunOrScheduleAction(Action, ...) overload, except invoking
            // box.Invoke instead of action().

            ref Task? currentTask = ref Task.t_currentTask;
            Task? prevCurrentTask = currentTask;



            // Otherwise, run it, making sure that t_currentTask is null'd out appropriately during the execution
            {
                if (prevCurrentTask != null) currentTask = null;
                box.MoveNext();
            }
            {
                if (prevCurrentTask != null) currentTask = prevCurrentTask;
            }
        }

        /// <summary>Schedules the action to be executed.  No ExecutionContext work is performed used.</summary>
        /// <param name="action">The action to invoke or queue.</param>
        /// <param name="task">The task scheduling the action.</param>
        internal static void UnsafeScheduleAction(Action action, Task? task)
        {
            AwaitTaskContinuation atc = new AwaitTaskContinuation(action, flowExecutionContext: false);

            //Debug.Fail("we really shouldn't get in threadpool");
            action();
            //ThreadPool.UnsafeQueueUserWorkItemInternal(atc, preferLocal: true);
        }
    }
}