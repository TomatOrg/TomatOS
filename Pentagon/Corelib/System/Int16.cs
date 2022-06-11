namespace System;

public readonly struct Int16
{

    public const short MaxValue = 32767;
    public const short MinValue = -32768;
    
#pragma warning disable 169
    private readonly short _value;
#pragma warning restore 169
    
    public override bool Equals(object obj)
    {
        if (obj is short value)
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