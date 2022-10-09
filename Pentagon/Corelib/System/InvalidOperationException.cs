namespace System;

public class InvalidOperationException : SystemException
{

    internal const string CollectionCorrupted = "A prior operation on this collection was interrupted by an exception. Collection's state is no longer trusted.";
    internal const string ReadOnly = "Instance is read-only.";
    
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