namespace System
{
    public sealed class ExecutionEngineException : SystemException
    {
        
        public ExecutionEngineException()
            : base("Internal error in the runtime.")
        {
        }

        public ExecutionEngineException(string message)
            : base(message)
        {
        }

        public ExecutionEngineException(string message, Exception innerException)
            : base(message, innerException)
        {
        }
        
    }
}