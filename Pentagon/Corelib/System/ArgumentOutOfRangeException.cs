namespace System;

public class ArgumentOutOfRangeException : ArgumentException
{

    internal const string NeedNonNegNum = "Non-negative number required.";
    internal const string BiggerThanCollection = "Must be less than or equal to the size of the collection.";
    internal const string IndexMustBeLess = "Index was out of range. Must be non-negative and less than the size of the collection.";
    internal const string IndexMustBeLessOrEqual = "Index was out of range. Must be non-negative and less than or equal to the size of the collection.";
    internal const string SmallCapacity = "capacity was less than the current size.";
    internal const string Count = "Count must be positive and count must refer to a location within the string/array/collection.";

    public virtual object ActualValue { get; }

    public override string Message
    {
        get
        {
            var s = base.Message;
            if (ActualValue == null) 
                return s;

            var valueMessage = string.Concat("Actual value was ", ActualValue, ".");
            return s == null ? valueMessage : $"{s}\n{valueMessage}";
        }
    }

    public ArgumentOutOfRangeException()
        : base("Specified argument was out of the range of valid values.")
    {
    }

    public ArgumentOutOfRangeException(string paramName)
        : base(paramName)
    {
    }

    public ArgumentOutOfRangeException(string paramName, string message)
        : base(message, paramName)
    {
    }

    public ArgumentOutOfRangeException(string message, Exception innerException)
        : base(message, innerException)
    {
    }

    public ArgumentOutOfRangeException(string paramName, object actualValue, string message)
        : base(message, paramName)
    {
        ActualValue = actualValue;
    }
}