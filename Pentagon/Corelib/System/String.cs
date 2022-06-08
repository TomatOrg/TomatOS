using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace System;

[StructLayout(LayoutKind.Sequential)]
public class String
{

    /// <summary>
    /// Represents the empty string. This field is read-only.
    /// </summary>
    public static readonly string Empty = "";

    private readonly int _length;
    private readonly char _chars;

    /// <summary>
    /// Gets the number of characters in the current String object.
    /// </summary>
    public int Length => _length;
    
    /// <summary>
    /// Indicates whether the specified string is null or an empty string ("").
    /// </summary>
    /// <param name="value">The string to test.</param>
    /// <returns>true if the value parameter is null or an empty string (""); otherwise, false.</returns>
    public static bool IsNullOrEmpty(string value)
    {
        return value == null || value.Length == 0;
    }

}