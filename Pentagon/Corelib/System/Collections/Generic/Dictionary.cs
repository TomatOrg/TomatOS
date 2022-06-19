namespace System.Collections.Generic;

public class Dictionary<TKey, TValue>
{

    private class Chain
    {
        public KeyValuePair<TKey, TValue> Entry;
        public Chain Next;
    }
    
    private Chain[][] _table;
    private int _capacity;
    private int _size;
    
    private void Add(TKey key, TValue value)
    {
        
    }
    
}