namespace System;

public class ArgumentException : SystemException
{

    internal const string LongerThanSrcString = "Source string was not long enough. Check sourceIndex and count.";
    internal const string EmptyName = "Empty name is not legal.";
    internal const string StringZeroLength = "String cannot have zero length.";
    internal const string StringComparison = "The string comparison type passed in is currently not supported.";

    internal const string InvalidGroupSize =
        "Every element in the value array should be between one and nine, except for the last element, which can be zero.";
    
    internal const string InvalidNativeDigitValue =
        "Each member of the NativeDigits array must be a single text element (one or more UTF16 code points) with a Unicode Nd (Number, Decimal Digit) property indicating it is a digit.";
    
    internal const string BufferNotFromPool =
        "The buffer is not associated with this pool and may not be returned to it.";
    
    internal const string OffsetOut =
        "Either offset did not refer to a position in the string, or there is an insufficient length of destination character array.";
    
    public string ParamName { get; }

    
    public override string Message
    {
        get
        {
            var s = base.Message ?? "Value does not fall within the expected range.";
            if (!string.IsNullOrEmpty(ParamName))
            {
                s = $"{s} (Parameter '{ParamName}')";
            }

            return s;
        }
    }

    public ArgumentException()
        : base("Value does not fall within the expected range.")
    {
    }

    public ArgumentException(string message)
        : base(message)
    {
    }

    public ArgumentException(string message, Exception innerException)
        : base(message, innerException)
    {
    }

    public ArgumentException(string message, string paramName, Exception innerException)
        : base(message, innerException)
    {
        ParamName = paramName;
    }

    public ArgumentException(string message, string paramName)
        : base(message)
    {
        ParamName = paramName;
    }

}