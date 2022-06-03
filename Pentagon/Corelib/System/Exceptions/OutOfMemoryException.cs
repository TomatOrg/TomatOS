using System.Runtime.InteropServices;

namespace System;

[StructLayout(LayoutKind.Sequential)]
public class OutOfMemoryException : SystemException
{
        
    public OutOfMemoryException()
        : base("Insufficient memory to continue the execution of the program.")
    {
    }

    public OutOfMemoryException(string message)
        : base(message)
    {
    }

    public OutOfMemoryException(string message, Exception innerException)
        : base(message, innerException)
    {
    }
        
}