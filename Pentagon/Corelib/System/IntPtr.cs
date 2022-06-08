namespace System;

/// <summary>
/// A platform-specific type that is used to represent a pointer or a handle.
/// </summary>
public readonly struct IntPtr
{

    /// <summary>
    /// A read-only field that represents a pointer or handle that has been initialized to zero.
    /// </summary>
    public static readonly IntPtr Zero = new IntPtr(0);

    /// <summary>
    /// Gets the largest possible value of IntPtr.
    /// </summary>
    public static nint MaxValue => unchecked((nint)long.MaxValue);
    
    /// <summary>
    /// Gets the smallest possible value of IntPtr.
    /// </summary>
    public static nint MinValue => unchecked((nint)long.MinValue);


    /// <summary>
    /// Gets the size of this instance.
    /// </summary>
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

}