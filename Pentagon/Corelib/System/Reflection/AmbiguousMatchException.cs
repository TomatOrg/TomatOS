namespace System.Reflection;

public sealed class AmbiguousMatchException : SystemException
{
    
    public AmbiguousMatchException()
        : base("Ambiguous match found.")
    {
    }

    public AmbiguousMatchException(string message)
        : base(message)
    {
    }

    public AmbiguousMatchException(string message, Exception inner)
        : base(message, inner)
    {
    }
    
}