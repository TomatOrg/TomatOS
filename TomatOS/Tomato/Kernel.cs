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
using Rectangle = System.Drawing.Rectangle;

namespace Tomato;

public class Terminal
{
    // GUI data
    Memory<uint> memory;
    IFramebuffer framebuffer;

    // Font data
    Font font;
    FontBlitter fontBlitter;

    // Buffer of all the text.
    // TODO: limit scrollback
    List<List<char>> textBuffer = new();

    // Total lines that can fit on the screen
    int maxLines;
    // First line displayed on screen
    int firstLine = 0;
    // Current line the user is typing
    int currentLine = 0;
    // Current X coordinate in the current line
    int cursorX;

    // Keyboard state
    int shift = 0, altgr = 0;

    // blit backbuffer position (mx,my) to frontbuffer (sx,sy)
    void BlitHelper(int mx, int my, int sx, int sy, int w, int h) => framebuffer.Blit(mx + my * framebuffer.Width, new Rectangle(sx, sy, w, h));

    // The code works by NEVER scrolling the frontbuffer
    // instead on a newline it replaces the line that isn't visible anymore, with the new line to show
    // Hence we need to keep track of where to put text on the backbuffer, and where is it going to be shown, like this
    /*
        A  A      >H  B       H  C
        B  B       B  C      >I  D
        C  C       C  D       C  E
        D  D       D  E       D  F
        E  E       E  F       E  G
        F  F       F  G       F  H
       >G >G       G >H       G >I */
    // Note that the Y needs to account for the descender (we pass the cursor position to the FotnBlitter, not top left), otherwise characters get cut off
    int TopYMemory(int line) => (line % maxLines) * font.Size;
    int TopYScreen(int line) => (line - firstLine) * font.Size;
    int CursorYMemory(int line) => TopYMemory(line) + font.Size - font.Descender;
    int CursorYScreen(int line) => TopYScreen(line) + font.Size - font.Descender;

    void Scroll(int scrollBy)
    {
        firstLine += scrollBy;
        // if we're scrolling upwards, the first line is not going to be in the backbuffer, so render it
        if (scrollBy < 0)
        {
            // TODO: support scrollBy != -1 here
            // TODO: make a DrawChar
            var m = memory.Slice(TopYMemory(firstLine) * framebuffer.Width);
            for (int i = 0; i < font.Size; i++) m.Slice(i * framebuffer.Width, framebuffer.Width).Span.Fill(0);
            int x = 0;
            foreach (var c in textBuffer[firstLine])
            {
                var g = font.Glyphs[c - font.First];
                fontBlitter.DrawChar(c, x, CursorYMemory(firstLine));
                x += g.Advance;
            }
        }

        var idx = (firstLine + maxLines - 1) % maxLines;
        // The cursor is the starting index to read memory like a ring buffer of lines
        // so you start copying at idx+1 in memory to 0 in screen
        var screenLine = 0;

        // - all the lines that in memory are below the cursor, actually are the first lines
        Blit(idx + 1, maxLines - (1 + idx));
        // - then you handle wraparound, adding the lines that are above the cursor
        // if we are scrolling downwards, clean the line
        if (scrollBy > 0)
        {
            var m = memory.Slice(idx * font.Size * framebuffer.Width);
            for (int i = 0; i < font.Size; i++) m.Slice(i * framebuffer.Width, framebuffer.Width).Span.Fill(0);
        }
        Blit(0, idx + 1);

        framebuffer.Flush();

        void Blit(int memY, int lines)
        {
            BlitHelper(0, memY * font.Size, 0, screenLine * font.Size, framebuffer.Width, lines * font.Size);
            screenLine += lines;
        }
    }

    void HandleNewline()
    {
        cursorX = 0;
        currentLine++;
        textBuffer.Add(new List<char>());

        // time to scroll everything one line up
        if (currentLine >= maxLines) Scroll(1);
    }

    void HandleBackspace()
    {
        var line = textBuffer[currentLine];
        if (line.Count == 0)
        {
            if (currentLine != 0)
            {
                // pop the last line
                textBuffer.RemoveAt(currentLine);
                currentLine--;
                line = textBuffer[currentLine];

                // advance cursor to the last character in the line
                cursorX = 0;
                foreach (var c in line) cursorX += font.Glyphs[c - font.First].Advance;

                // scrolling is only needed when there is more text than fits on the screen
                if (firstLine > 0) Scroll(-1);
            }
        }
        else if (line.Count > 0)
        {
            // pop the last character from the line
            // TODO: use ^1 when System.Index is supported
            var ch = line[line.Count - 1];
            line.RemoveAt(line.Count - 1);

            // get information on the popped glyph. PlaneBounds is the rectangle occupied by it, relative to the cursor position
            var g = font.Glyphs[ch - font.First];
            var pb = g.PlaneBounds;

            // pb represents the character to delete, so move the cursor back to where it was when that character was added, so the rectangle X and Y match
            cursorX -= g.Advance;

            // clear where the old character was
            var fbSlice = memory.Slice(cursorX + pb.X + (CursorYMemory(currentLine) + pb.Y) * framebuffer.Width);
            for (int i = 0; i < pb.Height; i++) fbSlice.Slice(i * framebuffer.Width, pb.Width).Span.Fill(0);
            BlitHelper(cursorX + pb.X, CursorYMemory(currentLine) + pb.Y, cursorX + pb.X, CursorYScreen(currentLine) + pb.Y, pb.Width, pb.Height);

            framebuffer.Flush();
        }
    }
    void HandleCharInsert(char c)
    {
        textBuffer[currentLine].Add(c);

        var g = font.Glyphs[(int)c - font.First];
        var pb = g.PlaneBounds;
        fontBlitter.DrawChar(c, cursorX, CursorYMemory(currentLine));

        BlitHelper(cursorX + pb.X, CursorYMemory(currentLine) + pb.Y, cursorX + pb.X, CursorYScreen(currentLine) + pb.Y, pb.Width, pb.Height);
        framebuffer.Flush();

        cursorX += g.Advance;
    }
    void KeyboardHandler(KeyEvent k)
    {
        if (!k.Released && (k.Code == KeyCode.LeftShift || k.Code == KeyCode.RightShift)) { shift++; return; }
        if (k.Released && (k.Code == KeyCode.LeftShift || k.Code == KeyCode.RightShift)) { shift--; return; }

        if (!k.Released && (k.Code == KeyCode.RightAlt)) { altgr++; return; }
        if (k.Released && (k.Code == KeyCode.RightAlt)) { altgr--; return; }

        if (k.Released) return;

        if (k.Code == KeyCode.Enter) HandleNewline();
        else if (k.Code == KeyCode.Backspace) HandleBackspace();
        else
        {
            // get codepoint, with all the appropriate checks
            if (((int)k.Code) > 0x200) return;
            var c = Kernel.GetCodepoint(k.Code, shift > 0, altgr > 0);
            if (c < font.First || c >= (font.First + font.Glyphs.Length)) return;

            HandleCharInsert((char)c);
        }
    }
    public Terminal(IFramebuffer fb, Memory<uint> m, IKeyboard kbd, Font f)
    {
        font = f;
        memory = m;
        framebuffer = fb;

        fontBlitter = new FontBlitter(font, memory, framebuffer.Width, framebuffer.Height, 0xFFFFFFFF);
        textBuffer.Add(new List<char>());

        maxLines = framebuffer.Height / font.Size;

        kbd.RegisterCallback(KeyboardHandler);
    }
}

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

    static Terminal term;
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
        var framebuffer = dev.CreateFramebuffer(output.Width, output.Height);
        output.SetFramebuffer(framebuffer, new Rectangle(0, 0, output.Width, output.Height));
        // allocate the framebuffer
        var m = new byte[framebuffer.Width * framebuffer.Height * 4].AsMemory();
        var memory = MemoryMarshal.Cast<byte, uint>(m);
        framebuffer.Backing = m;

        // Create a terminal spanning the whole screen
        term = new Terminal(framebuffer, memory, PS2.Keyboard, new Font(Typeface.Default, 16));
        return 0;
    }

}