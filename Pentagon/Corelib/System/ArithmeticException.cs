using System.Runtime.InteropServices;

namespace System;

[StructLayout(LayoutKind.Sequential)]
public class ArithmeticException : SystemException
{
        
    public ArithmeticException()
        : base("Overflow or underflow in the arithmetic operation.")
    {
    }

    public ArithmeticException(string message)
        : base(message)
    {
    }

    public ArithmeticException(string message, Exception innerException)
        : base(message, innerException)
    {
    }
        
}