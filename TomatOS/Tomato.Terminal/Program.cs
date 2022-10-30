using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Runtime.InteropServices;
using Tomato.Graphics;
using Tomato.Hal.Interfaces;
using Tomato.Hal.Managers;

namespace Tomato.Terminal;

internal static class Program
{

    private static DisplayManager _displayManager;
    private static Terminal _terminal;
    
    public static void Main()
    {
        try
        {
            _displayManager = DisplayManager.Claim();
        }
        catch (InvalidOperationException)
        {
            Debug.Print("Failed to claim the display manager!");
            return;
        }

        // get the graphics device
        var dev = _displayManager.GraphicsDevices[0];
        var output = dev.Outputs[0];
        
        // create a gpu framebuffer and attach it to the first output
        var framebuffer = dev.CreateFramebuffer(output.Width, output.Height);
        output.SetFramebuffer(framebuffer, new Rectangle(0, 0, output.Width, output.Height));
        
        // allocate a cpu backing 
        var m = new byte[framebuffer.Width * framebuffer.Height * 4].AsMemory();
        var memory = MemoryMarshal.Cast<byte, uint>(m);
        framebuffer.Backing = m;

        var kbd = Tomato.Hal.Hal.Keyboard;
        _terminal = new Terminal(framebuffer, memory, kbd, new Font(Typeface.Default, 16));
        _terminal.InsertLine("Hello world from our awesome terminal.");
        _terminal.InsertLine("Welcome to TomatOS!");
    }

}