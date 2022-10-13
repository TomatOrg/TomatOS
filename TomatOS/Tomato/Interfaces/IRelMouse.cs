using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using Tomato.Gui;

namespace Tomato.Interfaces
{
    public class RelMouseEvent : GuiEvent
    {
        public int deltaX, deltaY;
        public bool leftPressed, rightPressed;
        public RelMouseEvent(int dx, int dy, bool left, bool right)
        {
            deltaX = dx;
            deltaY = dy;
            leftPressed = left;
            rightPressed = right;
        }
    }
    
    public interface IRelMouse
    {
        public abstract void RegisterCallback(Action<RelMouseEvent> callback);
    }
}
