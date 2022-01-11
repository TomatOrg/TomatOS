namespace System
{
    public class IndexOutOfRangeException : SystemException
    {
        
        public IndexOutOfRangeException()
            : base("Index was outside the bounds of the array.")
        {
        }

        public IndexOutOfRangeException(string message)
            : base(message)
        {
        }

        public IndexOutOfRangeException(string message, Exception innerException)
            : base(message, innerException)
        {
        }
        
    }
}