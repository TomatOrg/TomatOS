namespace System;

public class ApplicationException : Exception
{
    
    public ApplicationException()
        : base("Cannot access a disposed object.")
    {
    }

    public ApplicationException(string message)
        : base(message)
    {
    }

    public ApplicationException(string message, Exception innerException)
        : base(message, innerException)
    {
    }
    
}