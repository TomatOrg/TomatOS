namespace System;

public class ObjectDisposedException : InvalidOperationException
{
    
    public ObjectDisposedException()
        : base("Cannot access a disposed object.")
    {
    }

    public ObjectDisposedException(string message)
        : base(message)
    {
    }

    public ObjectDisposedException(string message, Exception innerException)
        : base(message, innerException)
    {
    }
    
}