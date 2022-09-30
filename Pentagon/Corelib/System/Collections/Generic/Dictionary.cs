namespace System.Collections.Generic;

public class Dictionary<TKey, TValue>
{

    List<KeyValuePair<TKey, TValue>> array;

    public Dictionary()
    {
        array = new(0);
    }

    public bool ContainsKey(TKey key)
    {
        for (int i = 0; i < array.Count; i++)
        {
            var e = array[i];
            if (e.Key.Equals(key)) // TODO: this is broken
            {
                return true;
            }
        }
        
        return false;
    }

    public TValue this[TKey key]
    {
        get {
            for (int i = 0; i < array.Count; i++)
            {
                var e = array[i];
                if (e.Key.Equals(key)) // TODO: this is broken
                {
                    return e.Value;
                }
            }
            return default; // TODO:
        }
        set {
            for (int i = 0; i < array.Count; i++)
            {
                var e = array[i];
                if (e.Key.Equals(key)) // TODO: this is broken
                {
                    array[i] = new(e.Key, value);
                    return;
                }
            }
            array.Add(new(key, value));
        }
    }

}