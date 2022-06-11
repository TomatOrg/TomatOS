using System.Runtime.InteropServices;

namespace System;

[StructLayout(LayoutKind.Sequential)]
public class NullReferenceException : SystemException
{
     
    public NullReferenceException()
        : base("Object reference not set to an instance of an object.")
    {
    }

    public NullReferenceException(string message)
        : base(message)
    {
    }

    public NullReferenceException(string message, Exception innerException)
        : base(message, innerException)
    {
    }
        
}