using System.Runtime.InteropServices;

namespace System;

[StructLayout(LayoutKind.Sequential)]
public class SystemException : Exception
{

    private const string DefaultMessage = "System error.";
    
    public SystemException()
        : base(DefaultMessage)
    {
    }

    public SystemException(string message)
        : base(message ?? DefaultMessage)
    {
    }

    public SystemException(string message, Exception innerException)
        : base(message ?? DefaultMessage, innerException)
    {
    }
        
}