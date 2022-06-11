namespace System;

public readonly struct Byte
{

    public const byte MaxValue = 255;
    public const byte MinValue = 0;
    
#pragma warning disable 169
    private readonly byte _value;
#pragma warning restore 169
    
    public override bool Equals(object obj)
    {
        if (obj is byte value)
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