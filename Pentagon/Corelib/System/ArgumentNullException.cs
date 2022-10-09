namespace System;

public class ArgumentNullException : ArgumentException
{

    internal const string String = "String reference not set to an instance of a String.";
    internal const string Array = "Array cannot be null.";
    internal const string ArrayValue = "Found a null value within an array.";

    public ArgumentNullException()
        : base("Value cannot be null.")
    {
    }

    public ArgumentNullException(string paramName)
        : base("Value cannot be null.", paramName)
    {
    }

    public ArgumentNullException(string message, Exception innerException)
        : base(message, innerException)
    {
    }

    public ArgumentNullException(string paramName, string message)
        : base(message, paramName)
    {
    }
    
}