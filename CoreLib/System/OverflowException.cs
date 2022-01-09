namespace System
{
    public class OverflowException : ArithmeticException
    {
        
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
}