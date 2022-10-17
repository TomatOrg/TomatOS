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

    private static DisplayManager _displayManager = null;
    private static Memory<uint> _memory = Memory<uint>.Empty;
    
    private static IFramebuffer _framebuffer = null;
    private static Font _font = null;
    private static FontBlitter _fontBlitter;
    
    private static int _cellX = 0;
    private static int _cellY = 0;
    private static bool _shift = false;
    private static bool _altgr = false;
    
    private static List<List<char>> textBuffer = new();

    private static void BlitFlush() {
        _framebuffer.Blit(0, new System.Drawing.Rectangle(0, 0, _framebuffer.Width, _framebuffer.Height));
        _framebuffer.Flush();
    }
    
    private static void KeyboardHandler(in KeyEvent k)
    {
        switch (k.Released)
        {
            // handle shift keys
            case false when k.Code is KeyCode.LeftShift or KeyCode.RightShift:
                _shift = true;
                return;
            case true when k.Code is KeyCode.LeftShift or KeyCode.RightShift:
                _shift = false;
                return;
            
            // handle alt keys
            case false when k.Code is KeyCode.RightAlt:
                _altgr = true;
                return;
            case true when k.Code is KeyCode.RightAlt:
                _altgr = false;
                return;

            // key released, don't care
            case true:
                return;

            // key pressed, handle it 
            case false:
            {
                switch (k.Code)
                {
                    // enter key causes us to go down
                    case KeyCode.Enter:
                    {
                        
                    } break;

                    // backspace key causes us to go back
                    case KeyCode.Backspace:
                    {
                        
                    } break;

                    // just a normal key
                    default:
                    {
                        if (((int)k.Code) > 0x200) return;
                        var c = Kernel.GetCodepoint(k.Code, shift > 0, altgr > 0);
                        if (c < font.First || c >= (font.First + font.Glyphs.Length)) return;
            
                        textBuffer[textBuffer.Count - 1].Add((char)c);

                        char[] chars = new char[1];
                        chars[0] = (char)c;
                        var size = font.Glyphs[c - font.First].Advance;

                        fontBlitter.DrawString(new string(chars), cellX, cellY);
                        BlitFlush();

                        cellX += size;
                    } break;
                }  
            } break;
        }
    }
    
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
        _framebuffer = dev.CreateFramebuffer(output.Width, output.Height);
        output.SetFramebuffer(_framebuffer, new Rectangle(0, 0, output.Width, output.Height));
        
        // allocate a cpu backing 
        var m = new byte[_framebuffer.Width * _framebuffer.Height * 4].AsMemory();
        _memory = MemoryMarshal.Cast<byte, uint>(m);
        _framebuffer.Backing = m;
        
        // TODO: load the font...
        _fontBlitter = new FontBlitter(_font, _memory, _framebuffer.Width, _framebuffer.Height, (uint)Color.White.ToArgb());
        textBuffer.Add(new List<char>());
        _cellX = 0;
        _cellY = _font.Size;

        // register a callback for all the connected keyboards
        foreach (var keyboard in _displayManager.Keyboards)
        {
            keyboard.RegisterCallback(KeyboardHandler);
        }
    }

}