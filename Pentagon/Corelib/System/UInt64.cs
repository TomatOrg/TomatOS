namespace System;

public readonly struct UInt64
{
#pragma warning disable 169
    private readonly ulong _value;
#pragma warning restore 169
    
    public const ulong MaxValue = 18446744073709551615;
    public const ulong MinValue = 0;
    
    public override bool Equals(object obj)
    {
        if (obj is ulong value)
        {
            return _value == value;
        }
        return false;
    }

    public override int GetHashCode()
    {
        return (int)_value ^ (int)(_value >> 32);
    }

    public override string ToString()
    {
        return "";
    }
    
}