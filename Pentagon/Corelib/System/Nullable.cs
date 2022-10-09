using System.Runtime.InteropServices;

namespace System;

[StructLayout(LayoutKind.Sequential)]
public struct Nullable<T> where T : struct
{

    private readonly bool _hasValue;
    internal T _value;

    public bool HasValue => _hasValue;

    public T Value
    {
        get
        {
            if (!_hasValue)
                throw new InvalidOperationException("Nullable object must have a value.");
            return _value;
        }
    }
    
    public Nullable(T value)
    {
        _hasValue = true;
        _value = value;
    }

    public readonly T GetValueOrDefault()
    {
        return _value;
    }

    public readonly T GetValueOrDefault(T value)
    {
        return _hasValue ? _value : value;
    }

    public override int GetHashCode()
    {
        return _hasValue ? _value.GetHashCode() : 0;
    }
    
    public override string ToString()
    {
        return _hasValue ? _value.ToString() : string.Empty;
    }

    public override bool Equals(object obj)
    {
        if (!_hasValue)
            return obj == null;

        return obj != null && _value.Equals(obj);
    }

    public static explicit operator T(T? value)
    {
        // ReSharper disable once PossibleInvalidOperationException
        return value.Value;
    }
    
    public static implicit operator T?(T value)
    {
        return value;
    }
    
}