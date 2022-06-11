namespace System;

public readonly struct UInt32
{
#pragma warning disable 169
    private readonly uint _value;
#pragma warning restore 169
    
    public override bool Equals(object obj)
    {
        if (obj is uint value)
        {
            return _value == value;
        }
        return false;
    }
    
    public override int GetHashCode()
    {
        return (int)_value;
    }

    public override string ToString()
    {
        return "";
    }

    
}