namespace System;

public readonly struct SByte
{
#pragma warning disable 169
    private readonly sbyte _value;
#pragma warning restore 169
    
    public override bool Equals(object obj)
    {
        if (obj is sbyte value)
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