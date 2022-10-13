using System;
using System.Drawing;
using Tomato.Interfaces;

namespace Tomato.Drivers.Graphics.Plain;

internal class PlainGraphicsOutput : IGraphicsOutput
{

    public int Width { get; }
    public int Height { get; }

    internal int RowBytes;
    internal Memory<byte> Address;
    internal Rectangle Rectangle;
    internal PlainFramebuffer Framebuffer;

    public PlainGraphicsOutput(int width, int height, int rowBytes, Memory<byte> framebuffer)
    {
        Width = width;
        Height = height;
        RowBytes = rowBytes;
        Address = framebuffer;
    }
    
    public void SetFramebuffer(IFramebuffer framebuffer, in Rectangle rect)
    {
        // save the new rect
        Rectangle = rect;
        
        // if the same framebuffer nothing else to do
        if (Framebuffer == framebuffer)
            return;
        
        // get it as a plain one 
        var fb = (PlainFramebuffer)framebuffer;
        Framebuffer = fb;
        
        // if we don't have it then update it 
        if (!fb.Outputs.Contains(this))
        {
            fb.Outputs.Add(this);
        }
    }
}
