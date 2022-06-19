using System.Collections;
using System.Collections.Generic;

namespace System;

public sealed class CharEnumerator : IEnumerator<char>
{
    public char Current
    {
        get
        {
            if (_index == -1) throw new InvalidOperationException("Enumeration has not started. Call MoveNext.");
            if (_index >= _str!.Length) throw new InvalidOperationException("Enumeration already finished.");
            return _currentElement;
        }
    }
    
    object IEnumerator.Current => Current;

    private string _str;
    private int _index;
    private char _currentElement;
    
    internal CharEnumerator(string str)
    {
        _str = str;
        _index = -1;
    }
    
    public bool MoveNext()
    {
        if (_index < _str.Length - 1)
        {
            _index++;
            _currentElement = _str[_index];
            return true;
        }
        _index = _str.Length;
        return false;   
    }

    public void Reset()
    {
        _currentElement = (char)0;
        _index = -1;
    }

    public void Dispose()
    {
        if (_str != null)
        {
            _index = _str.Length;
        }
        
        _str = null;
    }
}