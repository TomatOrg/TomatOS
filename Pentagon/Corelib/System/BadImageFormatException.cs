namespace System;

public class BadImageFormatException : SystemException
{
    
    public BadImageFormatException()
        : base("Format of the executable (.exe) or library (.dll) is invalid.")
    {
    }

    public BadImageFormatException(string message)
        : base(message)
    {
    }

    public BadImageFormatException(string message, Exception innerException)
        : base(message, innerException)
    {
    }

}