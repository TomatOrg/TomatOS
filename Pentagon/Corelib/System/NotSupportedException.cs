namespace System;

public class NotSupportedException : SystemException
{

    internal const string ReadOnlyCollection = "Collection is read-only.";
    internal const string CannotCallEqualsOnSpan = "Equals() on Span and ReadOnlySpan is not supported. Use operator== instead.";
    internal const string CannotCallGetHashCodeOnSpan = "GetHashCode() on Span and ReadOnlySpan is not supported.";

    public NotSupportedException()
        : base("Specified method is not supported.")
    {
    }
    
    public NotSupportedException(string message)
        : base(message)
    {
    }
    
    public NotSupportedException(string message, Exception innerException)
        : base(message, innerException)
    {
    }

}