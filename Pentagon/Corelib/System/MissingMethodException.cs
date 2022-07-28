namespace System;

public class MissingMethodException : MissingMemberException
{
    
    public override string Message => _className == null ? base.Message : $"Method '{_className}.{_memberName}' not found.";

    public MissingMethodException()
        : base("Attempted to access a missing method.")
    {
    }

    public MissingMethodException(string message)
        : base(message)
    {
    }

    public MissingMethodException(string message, Exception innerException)
        : base(message, innerException)
    {
    }

    public MissingMethodException(string className, string memberName)
        : base(className, memberName)
    {
    }
    
}