namespace System;

public class ArgumentException : SystemException
{

    public string ParamName { get; }

    
    public override string Message
    {
        get
        {
            var s = base.Message ?? "Value does not fall within the expected range.";
            if (!string.IsNullOrEmpty(ParamName))
            {
                s = $"{s} (Parameter '{ParamName}')";
            }

            return s;
        }
    }

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

    public ArgumentException(string message, string paramName, Exception innerException)
        : base(message, innerException)
    {
        ParamName = paramName;
    }

    public ArgumentException(string message, string paramName)
        : base(message)
    {
        ParamName = paramName;
    }

}