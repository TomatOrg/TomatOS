using System.Runtime.InteropServices;

namespace System;

[StructLayout(LayoutKind.Sequential)]
public class OverflowException : ArithmeticException
{

    internal const string NegateTwosCompNum = "Negating the minimum value of a twos complement number is invalid.";
    
    public OverflowException()
        : base("Arithmetic operation resulted in an overflow.")
    {
    }

    public OverflowException(string message)
        : base(message)
    {
    }

    public OverflowException(string message, Exception innerException)
        : base(message, innerException)
    {
    }
        
}