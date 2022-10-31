using System;

namespace Tomato.Hal.Interfaces;

/// <summary>
/// Represents a key event
/// </summary>
public struct KeyEvent
{
    public int Code { get; }
    public bool Released { get; }
    
    public KeyEvent(int c, bool r)
    {
        Code = c;
        Released = r;
    }
}

public delegate void KeyboardCallback(in KeyEvent key);

public interface IKeyboard
{
    public void RegisterCallback(KeyboardCallback callback);
}