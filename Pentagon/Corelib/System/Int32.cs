namespace System;

public readonly struct Int32
{
    
    public const int MaxValue = 2147483647;
    public const int MinValue = -2147483648;
    
#pragma warning disable 169
    private readonly int _value;
#pragma warning restore 169
    
    public override bool Equals(object obj)
    {
        if (obj is int value)
        {
            return _value == value;
        }
        return false;
    }

    public override int GetHashCode()
    {
        return _value;
    }

    public override string ToString()
    {
        return "";
    }
    
}