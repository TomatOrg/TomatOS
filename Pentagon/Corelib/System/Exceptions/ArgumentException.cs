namespace System;

public class ArgumentException : SystemException
{

    public string ParamName { get; }
    
    public ArgumentException()
        : base("Value does not fall within the expected range.")
    {
    }

    public ArgumentException(string message)
        : base(message)
    {
    }

    public ArgumentException(string message, Exception innerException)
        : base(message, innerException)
    {
    }
    
    public ArgumentException(string message, string paramName)
        : base(message)
    {
        ParamName = paramName;
    }

    
}