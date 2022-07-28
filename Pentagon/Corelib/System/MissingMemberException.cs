namespace System;

public class MissingMemberException : MemberAccessException
{

    protected readonly string _className;
    protected readonly string _memberName;

    public override string Message => _className == null ? base.Message : $"Member '{_className}.{_memberName}' not found.";

    public MissingMemberException()
        : base("Attempted to access a missing member.")
    {
    }

    public MissingMemberException(string message)
        : base(message)
    {
    }

    public MissingMemberException(string message, Exception innerException)
        : base(message, innerException)
    {
    }

    public MissingMemberException(string className, string memberName)
    {
        _className = className;
        _memberName = memberName;
    }

}