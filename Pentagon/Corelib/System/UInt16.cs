namespace System;

public readonly struct UInt16
{
#pragma warning disable 169
    private readonly ushort _value;
#pragma warning restore 169
    
    public override bool Equals(object obj)
    {
        if (obj is ushort value)
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