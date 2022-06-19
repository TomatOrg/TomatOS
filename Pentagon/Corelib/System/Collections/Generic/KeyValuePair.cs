namespace System.Collections.Generic;

public readonly struct KeyValuePair<TKey, TValue>
{
    
    public TKey Key { get; }
    public TValue Value { get; }
    
    public KeyValuePair(TKey key, TValue value)
    {
        Key = key;
        Value = value;
    }
    
    public void Deconstruct(out TKey key, out TValue value)
    {
        key = Key;
        value = Value;
    }

}