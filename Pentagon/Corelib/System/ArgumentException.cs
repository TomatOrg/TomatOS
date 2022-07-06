namespace System;

public class ArgumentException : SystemException
{

    private const string DefaultMessage = "Value does not fall within the expected range.";

    public string ParamName { get; }
    
    public ArgumentException()
        : base(DefaultMessage)
    {
    }

    public ArgumentException(string message)
        : base(message ?? DefaultMessage)
    {
    }

    public ArgumentException(string message, Exception innerException)
        : base(message ?? DefaultMessage, innerException)
    {
    }
    
    public ArgumentException(string message, string paramName)
        : base(message ?? DefaultMessage)
    {
        ParamName = paramName;
    }

    
}