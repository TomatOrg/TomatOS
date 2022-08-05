using System;
using System.Collections.Generic;
using System.Threading.Tasks;

namespace System.Threading
{
    internal class ExecutionContext
    {
        public delegate void ContextCallback(object? state);

        internal delegate void ContextCallback<TState>(ref TState state);
    }
}
