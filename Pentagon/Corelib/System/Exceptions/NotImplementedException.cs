namespace System;

public class NotImplementedException : SystemException
{
    
    public NotImplementedException()
        : base("The method or operation is not implemented.")
    {
    }

    public NotImplementedException(string message)
        : base(message)
    {
    }

    public NotImplementedException(string message, Exception innerException)
        : base(message, innerException)
    {
    }
    
}