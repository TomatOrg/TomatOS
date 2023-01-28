using System;

namespace Tomato.Hal.Interfaces;

/// <summary>
/// The event created on a relative mouse movement 
/// </summary>
public struct RelMouseEvent
{
        
    public int RelativeMovementX { get; }
    public int RelativeMovementY { get; }
    public bool LeftButton { get; }
    public bool RightButton { get; }
        
    public RelMouseEvent(int dx, int dy, bool left, bool right)
    {
        RelativeMovementX = dx;
        RelativeMovementY = dy;
        LeftButton = left;
        RightButton = right;
    }
}

public delegate void RelMouseCallback(in RelMouseEvent mouse);

/// <summary>
/// Represents a relative pointing device which can track movement delta
/// </summary>
public interface IRelMouse
{
    
    // TODO: information like polling rate or what not?
    
    /// <summary>
    /// Set a callback that is going to be called whenever an event is received.
    /// Note: This callback may block the input subsystem until this callback is returned!
    /// </summary>
    public RelMouseCallback Callback { set; }
    
}