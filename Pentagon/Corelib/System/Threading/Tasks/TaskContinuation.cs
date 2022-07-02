using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using System.Runtime.CompilerServices;

namespace System.Threading.Tasks
{
    public delegate void ContextCallback(object? state);

    internal abstract class TaskContinuation
    {
        internal abstract void Run(Task completedTask, bool canInlineContinuationTask);

        protected static void InlineIfPossibleOrElseQueue(Task task, bool needsProtection)
        {
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
                    //TODO:
                    //task.m_taskScheduler.InternalQueueTask(task);
                }
            }
            catch (Exception e)
            {
                //TaskSchedulerException tse = new TaskSchedulerException(e);
                //task.AddException(tse);
                task.Finish(false);
            }
        }
    }

    internal class AwaitTaskContinuation : TaskContinuation, IThreadPoolWorkItem
    {
        //private readonly ExecutionContext? m_capturedContext;
        protected readonly Action m_action;

        protected int m_continuationId;

        internal AwaitTaskContinuation(Action action, bool flowExecutionContext)
        {
            m_action = action;
            if (flowExecutionContext)
            {
                //m_capturedContext = ExecutionContext.Capture();
            }
        }

        protected Task CreateTask(Action<object?> action, object? state, TaskScheduler scheduler)
        {
            return new Task(
                action, state, null, default,
                TaskCreationOptions.None, InternalTaskOptions.QueuedByRuntime, scheduler)
            {
                //CapturedContext = m_capturedContext
            };
        }

        private static readonly ContextCallback s_invokeContextCallback = static (state) => ((Action)state)();
        protected static ContextCallback GetInvokeActionCallback() => s_invokeContextCallback;
        internal override void Run(Task task, bool canInlineContinuationTask)
        {
            //if (canInlineContinuationTask && IsValidLocationForInlining)
            //{
                RunCallback(GetInvokeActionCallback(), m_action, ref Task.t_currentTask);
            //}
            //else
            //{
            //    ThreadPool.UnsafeQueueUserWorkItemInternal(this, preferLocal: true);
            //}
        }

        
        void IThreadPoolWorkItem.Execute()
        {
            // TODO:
            m_action();
        }

        /*private static readonly ContextCallback s_invokeContextCallback = static (state) =>
        {
            Debug.Assert(state is Action);
            ((Action)state)();
        };*/
        private static readonly Action<Action> s_invokeAction = (action) => action();

        //protected static ContextCallback GetInvokeActionCallback() => s_invokeContextCallback;

        protected void RunCallback(ContextCallback callback, object? state, ref Task? currentTask)
        {
            Task? prevCurrentTask = currentTask;
            try
            {
                if (prevCurrentTask != null) currentTask = null;

                //ExecutionContext? context = m_capturedContext;
                //if (context == null)
                //{
                    callback(state);
                //}
                //else
                //{
                    //ExecutionContext.RunInternal(context, callback, state);
                //}
            }
            catch (Exception exception)
            {
                //Task.ThrowAsync(exception, targetContext: null);
            }
            finally
            {
                if (prevCurrentTask != null) currentTask = prevCurrentTask;
            }
        }

        internal static void RunOrScheduleAction(Action action, bool allowInlining)
        {
            ref Task? currentTask = ref Task.t_currentTask;
            Task? prevCurrentTask = currentTask;

            if (!allowInlining) //|| !IsValidLocationForInlining)
            {
                UnsafeScheduleAction(action, prevCurrentTask);
                return;
            }

            try
            {
                if (prevCurrentTask != null) currentTask = null;
                action();
            }
            catch (Exception exception)
            {
                //Task.ThrowAsync(exception, targetContext: null);
            }
            finally
            {
                if (prevCurrentTask != null) currentTask = prevCurrentTask;
            }
        }
        internal static void RunOrScheduleAction(IAsyncStateMachineBox box, bool allowInlining)
        {
            ref Task? currentTask = ref Task.t_currentTask;
            Task? prevCurrentTask = currentTask;

            if (!allowInlining) //|| !IsValidLocationForInlining)
            {
                //ThreadPool.UnsafeQueueUserWorkItemInternal(box, preferLocal: true);
                return;
            }

            try
            {
                if (prevCurrentTask != null) currentTask = null;
                box.MoveNext();
            }
            catch (Exception exception)
            {
                //Task.ThrowAsync(exception, targetContext: null);
            }
            finally
            {
                if (prevCurrentTask != null) currentTask = prevCurrentTask;
            }
        }
        internal static void UnsafeScheduleAction(Action action, Task? task)
        {
            AwaitTaskContinuation atc = new AwaitTaskContinuation(action, flowExecutionContext: false);
            //ThreadPool.UnsafeQueueUserWorkItemInternal(atc, preferLocal: true);
        }
    }
}
