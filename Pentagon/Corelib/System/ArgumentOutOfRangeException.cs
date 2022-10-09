namespace System;

public class ArgumentOutOfRangeException : ArgumentException
{

    internal const string StartIndex = "StartIndex cannot be less than zero.";
    internal const string IndexLength = "Index and length must refer to a location within the string.";
    internal const string Capacity = "Capacity exceeds maximum capacity.";
    internal const string SmallMaxCapacity = "MaxCapacity must be one or greater.";
    internal const string SmallCapacity = "capacity was less than the current size.";
    internal const string NegativeCapacity = "Capacity must be positive.";
    internal const string NegativeLength = "Length cannot be less than zero.";
    internal const string StartIndexLargerThanLength = "startIndex cannot be larger than length of string.";
    internal const string LengthGreaterThanCapacity = "The length cannot be greater than the capacity.";
    internal const string NegativeCount = "Count cannot be less than zero.";
    internal const string GenericPositive = "Value must be positive.";
    internal const string NegativeArgCount = "Argument count must not be negative.";
    internal const string NeedNonNegNum = "Non-negative number required.";
    internal const string IndexCountBuffer = "Index and count must refer to a location within the buffer.";
    internal const string IndexCount = "Index and count must refer to a location within the string.";
    internal const string NeedNonNegOrNegative1 = "Number must be either non-negative and less than or equal to Int32.MaxValue or -1.";
    internal const string Count = "Count must be positive and count must refer to a location within the string/array/collection.";

    internal const string Index =
        "Index was out of range. Must be non-negative and less than the size of the collection.";
    
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