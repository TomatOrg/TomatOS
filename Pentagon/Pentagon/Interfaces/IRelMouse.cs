using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using Pentagon.Gui;

namespace Pentagon.Interfaces
{
    internal class RelMouseEvent : GuiEvent
    {
        public int deltaX, deltaY;
        public bool leftPressed, rightPressed;
        internal RelMouseEvent(int dx, int dy, bool left, bool right)
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
