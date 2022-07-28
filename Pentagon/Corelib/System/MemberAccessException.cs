namespace System;

public class MemberAccessException : SystemException
{
    
    public MemberAccessException()
        : base("Cannot access member.")
    {
    }

    public MemberAccessException(string message)
        : base(message)
    {
    }

    public MemberAccessException(string message, Exception innerException)
        : base(message, innerException)
    {
    }
    
}