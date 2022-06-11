using System.Runtime.InteropServices;

namespace System;

[StructLayout(LayoutKind.Sequential)]
public class DivideByZeroException : ArithmeticException
{
        
    public DivideByZeroException()
        : base("Attempted to divide by zero.")
    {
    }

    public DivideByZeroException(string message)
        : base(message)
    {
    }

    public DivideByZeroException(string message, Exception innerException)
        : base(message, innerException)
    {
    }
        
}