using System.Runtime.InteropServices;

namespace System;

[StructLayout(LayoutKind.Sequential)]
public class SystemException : Exception
{

    public SystemException()
        : base("System error.")
    {
    }

    public SystemException(string message)
        : base(message)
    {
    }

    public SystemException(string message, Exception innerException)
        : base(message, innerException)
    {
    }
        
}