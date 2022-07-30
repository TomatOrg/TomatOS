namespace System;

public class InvalidOperationException : SystemException
{

    internal const string EnumFailedVersion = "Collection was modified; enumeration operation may not execute.";
    
    public InvalidOperationException()
        : base("Operation is not valid due to the current state of the object.")
    {
    }

    public InvalidOperationException(string message)
        : base(message)
    {
    }

    public InvalidOperationException(string message, Exception innerException)
        : base(message, innerException)
    {
    }
    
}