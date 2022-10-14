using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Numerics;
using System.Diagnostics.CodeAnalysis;
using System.Drawing;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Tomato.Resources;
using Tomato.Drivers;
using Tomato.Drivers.Graphics.Plain;
using Tomato.DriverServices;
using Tomato.DriverServices.Acpi;
using Tomato.Graphics;
using Tomato.Gui;
using Tomato.Gui.Framework;
using Tomato.Gui.Server;
using Tomato.Gui.Widgets;
using Tomato.Interfaces;
using Rectangle = Tomato.Gui.Widgets.Rectangle;

namespace Tomato;

public class Kernel
{
    static internal Memory<byte> kbdLayout;

    // TODO: use Rune
    internal static int GetCodepoint(KeyCode c, bool shiftHeld, bool altGrHeld)
    {
        int off = (int)c;
        //if (off >= 0x200) throw exception
        if (shiftHeld) off |= 0x200;
        if (altGrHeld) off |= 0x400;
        var offset = MemoryMarshal.Cast<byte, ushort>(Kernel.kbdLayout).Span[off];
        // TODO: proper unicode
        int codepoint = Kernel.kbdLayout.Span[0x1000 + offset];
        if ((codepoint & 0xE0) == 0xC0) // twobyte UTF8
        {
            codepoint = (codepoint & 0x1F) << 6;
            codepoint |= Kernel.kbdLayout.Span[0x1001 + offset] & 0x3F;
        }
        return codepoint;
    }

    static int cellX, cellY;
    static int shift = 0, altgr = 0;
    static int lastCharacterSize = 0;
    static Font font;
    static Memory<uint> _memory;
    static FontBlitter fontBlitter;
    static IFramebuffer framebuffer;
    static List<List<char>> textBuffer = new();

    static void KeyboardHandler(KeyEvent k) {
        if (!k.Released && (k.Code == KeyCode.LeftShift || k.Code == KeyCode.RightShift)) { shift = 1; return; }
        if (k.Released && (k.Code == KeyCode.LeftShift || k.Code == KeyCode.RightShift)) { shift = 0; return; }

        if (!k.Released && (k.Code == KeyCode.RightAlt)) { altgr = 1; return; }
        if (k.Released && (k.Code == KeyCode.RightAlt)) { altgr = 0; return; }

        if (k.Released) return;

        if (k.Code == KeyCode.Enter)
        {
            cellX = 0;
            cellY += font.Size;
            textBuffer.Add(new List<char>());
        }
        else if (k.Code == KeyCode.Backspace)
        {
            var line = textBuffer[textBuffer.Count - 1];
            if (line.Count == 0)
            {
                if (textBuffer.Count == 1) return;
                textBuffer.RemoveAt(textBuffer.Count - 1);
                line = textBuffer[textBuffer.Count - 1];
                cellY -= font.Size;
                cellX = 0;
                foreach (var c in line) cellX += font.Glyphs[c - font.First].Advance;
                return;
            }
            var ch = line[line.Count - 1];
            line.RemoveAt(line.Count - 1);
            var size = font.Glyphs[ch - font.First].Advance;
            cellX -= size;
            
            var fbSlice = _memory.Slice(cellX + (cellY - font.Size) * framebuffer.Width);
            for (int i = 0; i < font.Size; i++) fbSlice.Slice(i * framebuffer.Width, size).Span.Fill(0);

            BlitFlush(cellX, cellY - font.Size, size, font.Size);
        }
        else
        {
            if (((int)k.Code) > (Kernel.kbdLayout.Length - 0x1000)) return;
            var c = Kernel.GetCodepoint(k.Code, shift > 0, altgr > 0);
            textBuffer[textBuffer.Count - 1].Add((char)c);

            char[] chars = new char[1];
            chars[0] = (char)c;
            var size = font.Glyphs[c - font.First].Advance;

            fontBlitter.DrawString(new string(chars), cellX, cellY);
            BlitFlush(cellX, cellY - font.Size, size, font.Size);

            cellX += size;
        }

        void BlitFlush(int x, int y, int w, int h) {
            framebuffer.Blit(x + y * framebuffer.Width, new System.Drawing.Rectangle(x, y, w, h));
            framebuffer.Flush();
        }
    } 

    public static int Main()
    {
        KernelUtils.GetKbdLayout(out ulong kbdAddr, out ulong kbdSize);
        kbdLayout = MemoryServices.Map(kbdAddr, (int)kbdSize);

        // setup the basic subsystems
        var acpi = new Acpi();
        // Pci.Scan(acpi);
        //
        // // register built-in drivers
        // VirtioDevice.Register();
        
        IoApic.Scan(acpi);
        PS2.Register(); // this is a misnomer, since it doesn't use ResourceManager yet, but we need AML for that
        
        // load the default font 
        Typeface.Load();
        
        // Create a plain graphics device (from a framebuffer) and 
        IGraphicsDevice dev = new PlainGraphicsDevice();
        var output = dev.Outputs[0];
        framebuffer = dev.CreateFramebuffer(output.Width, output.Height);
        output.SetFramebuffer(framebuffer, new System.Drawing.Rectangle(0, 0, output.Width, output.Height));
        
        // allocate the framebuffer
        var m = new byte[framebuffer.Width * framebuffer.Height * 4].AsMemory();
        _memory = MemoryMarshal.Cast<byte, uint>(m);
        framebuffer.Backing = m;

        var size = 16;
        
        font = new Font(Typeface.Default, size);
        fontBlitter = new FontBlitter(font, _memory, framebuffer.Width, framebuffer.Height, 0xFFFFFFFF);
        textBuffer.Add(new List<char>());

        cellX = 0;
        cellY = font.Size;
        
        PS2.Keyboard.RegisterCallback(KeyboardHandler);

        return 0;
    }

}