namespace System;

/// <summary>
/// A platform-specific type that is used to represent a pointer or a handle.
/// </summary>
public readonly struct IntPtr
{
    public static readonly IntPtr Zero = new IntPtr(0);

    public static nint MaxValue => unchecked((nint)long.MaxValue);
    public static nint MinValue => unchecked((nint)long.MinValue);
    
    public static int Size => sizeof(long);

#pragma warning disable 169
    private readonly nint _value;
#pragma warning restore 169

    public IntPtr(int value)
    {
        _value = value;
    }

    public IntPtr(long value)
    {
        _value = (nint)value;
    }
    
    public long ToInt64()
    {
        return _value;
    }

    public override bool Equals(object obj)
    {
        if (obj is IntPtr value)
        {
            return _value == value._value;
        }
        return false;
    }
    
    public override int GetHashCode()
    {
        return ToInt64().GetHashCode();
    }

    public override string ToString()
    {
        return ToInt64().ToString();
    }
    
}