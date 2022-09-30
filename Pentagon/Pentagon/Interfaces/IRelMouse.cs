using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using Pentagon.Gui;

namespace Pentagon.Interfaces
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
        internal abstract void RegisterCallback(Action<RelMouseEvent> callback);
    }
}
