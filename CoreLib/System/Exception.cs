namespace System
{
    public class Exception
    {

        private string _message;

        // TODO: Have a default string of $"Exception of type '{GetType().ToString()}' was thrown."
        public string Message => _message;
        
        // TODO: stack frame stuff
        public Exception InnerException { get; }
        
        public Exception()
        {
        }

        public Exception(string message)
        {
            _message = message;
        }

        public Exception(string message, Exception innerException)
        {
            _message = message;
            InnerException = innerException;
        }

    }
}