namespace System;

public class FormatException : SystemException
{

    internal const string InvalidString = "Input string was not in a correct format.";
    internal const string IndexOutOfRange = "Index (zero based) must be greater than or equal to zero and less than the size of the argument list.";
    internal const string BadFormatSpecifier = "Format specifier was invalid.";
    
    public FormatException()
        : base("One of the identified items was in an invalid format.")
    {
    }

    public FormatException(string? message)
        : base(message)
    {
    }

    public FormatException(string? message, Exception? innerException)
        : base(message, innerException)
    {
    }

    
}