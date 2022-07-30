namespace System.Collections.Generic;

public static class KeyValuePair
{
    // Creates a new KeyValuePair<TKey, TValue> from the given values.
    public static KeyValuePair<TKey, TValue> Create<TKey, TValue>(TKey key, TValue value)
    {
        return new KeyValuePair<TKey, TValue>(key, value);
    }

    /// <summary>Used by KeyValuePair.ToString to reduce generic code</summary>
    internal static string PairToString(object key, object value)
    {
        return $"[{string.Concat(key)}, {string.Concat(value)}]";
    }
}


public readonly struct KeyValuePair<TKey, TValue>
{
    
    public TKey Key { get; }
    public TValue Value { get; }

    public KeyValuePair(TKey key, TValue value)
    {
        Key = key;
        Value = value;
    }
    
    public override string ToString()
    {
        return KeyValuePair.PairToString(Key, Value);
    }

    public void Deconstruct(out TKey key, out TValue value)
    {
        key = Key;
        value = Value;
    }
}