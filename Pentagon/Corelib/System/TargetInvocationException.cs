namespace System;

public class TargetInvocationException : ApplicationException
{
 
    public TargetInvocationException(Exception innerException)
        : this("Exception has been thrown by the target of an invocation.", innerException)
    {
    }

    public TargetInvocationException(string message, Exception innerException)
        : base(message, innerException)
    {
    }

}