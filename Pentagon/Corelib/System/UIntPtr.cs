namespace System;

public readonly struct UIntPtr
{
#pragma warning disable 169
    private readonly nuint _value;
#pragma warning restore 169
    
    public override bool Equals(object obj)
    {
        if (obj is UIntPtr value)
        {
            return _value == value._value;
        }
        return false;
    }

    public ulong ToUInt64()
    {
        return _value;
    }
    
    public override int GetHashCode()
    {
        return ToUInt64().GetHashCode();
    }

    public override string ToString()
    {
        return ToUInt64().ToString();
    }
    
}