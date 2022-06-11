namespace System;

public class ArgumentOutOfRangeException : ArgumentException
{

    public virtual object ActualValue { get; }

    public ArgumentOutOfRangeException()
        : base("Specified argument was out of the range of valid values.")
    {
    }

    public ArgumentOutOfRangeException(string paramName)
        : base(paramName)
    {
    }

    public ArgumentOutOfRangeException(string paramName, string message)
        : base(message, paramName)
    {
    }

    public ArgumentOutOfRangeException(string message, Exception innerException)
        : base(message, innerException)
    {
    }

    public ArgumentOutOfRangeException(string paramName, string message, object actualValue)
        : base(message, paramName)
    {
        ActualValue = actualValue;
    }
}