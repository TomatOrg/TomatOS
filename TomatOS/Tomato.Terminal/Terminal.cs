using System;
using System.Collections.Generic;
using System.Drawing;
using Tomato.Graphics;
using Tomato.Hal.Interfaces;

namespace Tomato.Terminal;

public class Terminal
{
    // GUI data
    private readonly Memory<uint> _memory;
    private readonly IFramebuffer _framebuffer;

    // Font data
    private readonly Font _font;
    private FontBlitter _fontBlitter;

    // Buffer of all the text.
    // TODO: limit scrollback
    private readonly List<List<char>> _textBuffer = new();

    // Total lines that can fit on the screen
    private readonly int _maxLines;
    // First line displayed on screen
    private int _firstLine = 0;
    // Current line the user is typing
    private int _currentLine = 0;
    // Current X coordinate in the current line
    private int _cursorX;

    // Keyboard state
    private int _shift = 0, _altgr = 0;

    // blit backbuffer position (mx,my) to frontbuffer (sx,sy)
    private void BlitHelper(int mx, int my, int sx, int sy, int w, int h) => 
        _framebuffer.Blit(mx + my * _framebuffer.Width, new Rectangle(sx, sy, w, h));

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
    public int TopYMemory(int line) => (line % _maxLines) * _font.Size;
    public int TopYScreen(int line) => (line - _firstLine) * _font.Size;
    public int CursorYMemory(int line) => TopYMemory(line) + _font.Size - _font.Descender;
    private int CursorYScreen(int line) => TopYScreen(line) + _font.Size - _font.Descender;

    public void Scroll(int scrollBy)
    {
        _firstLine += scrollBy;
        // if we're scrolling upwards, the first line is not going to be in the backbuffer, so render it
        if (scrollBy < 0)
        {
            // TODO: support scrollBy != -1 here
            // TODO: make a DrawChar
            var m = _memory[(TopYMemory(_firstLine) * _framebuffer.Width)..];
            for (var i = 0; i < _font.Size; i++) 
                m.Slice(i * _framebuffer.Width, _framebuffer.Width).Span.Fill(0);
            var x = 0;
            foreach (var c in _textBuffer[_firstLine])
            {
                var g = _font.Glyphs[c - _font.First];
                _fontBlitter.DrawChar(c, x, CursorYMemory(_firstLine));
                x += g.Advance;
            }
        }

        var idx = (_firstLine + _maxLines - 1) % _maxLines;
        // The cursor is the starting index to read memory like a ring buffer of lines
        // so you start copying at idx+1 in memory to 0 in screen
        var screenLine = 0;

        // - all the lines that in memory are below the cursor, actually are the first lines
        Blit(idx + 1, _maxLines - (1 + idx));
        // - then you handle wraparound, adding the lines that are above the cursor
        // if we are scrolling downwards, clean the line
        if (scrollBy > 0)
        {
            var m = _memory[(idx * _font.Size * _framebuffer.Width)..];
            for (var i = 0; i < _font.Size; i++) 
                m.Slice(i * _framebuffer.Width, _framebuffer.Width).Span.Fill(0);
        }
        Blit(0, idx + 1);

        _framebuffer.Flush();

        void Blit(int memY, int lines)
        {
            BlitHelper(0, memY * _font.Size, 0, screenLine * _font.Size, _framebuffer.Width, lines * _font.Size);
            screenLine += lines;
        }
    }

    public void InsertNewLine()
    {
        _cursorX = 0;
        _currentLine++;
        _textBuffer.Add(new List<char>());

        // time to scroll everything one line up
        if (_currentLine >= _maxLines) 
            Scroll(1);
    }

    public void InsertBackspace()
    {
        var line = _textBuffer[_currentLine];
        if (line.Count == 0)
        {
            if (_currentLine != 0)
            {
                // pop the last line
                _textBuffer.RemoveAt(_currentLine);
                _currentLine--;
                line = _textBuffer[_currentLine];

                // advance cursor to the last character in the line
                _cursorX = 0;
                foreach (var c in line) _cursorX += _font.Glyphs[c - _font.First].Advance;

                // scrolling is only needed when there is more text than fits on the screen
                if (_firstLine > 0) Scroll(-1);
            }
        }
        else if (line.Count > 0)
        {
            // pop the last character from the line
            // TODO: use ^1 when System.Index is supported
            var ch = line[line.Count - 1];
            line.RemoveAt(line.Count - 1);

            // get information on the popped glyph. PlaneBounds is the rectangle occupied by it, relative to the cursor position
            var g = _font.Glyphs[ch - _font.First];
            var pb = g.PlaneBounds;

            // pb represents the character to delete, so move the cursor back to where it was when that character was added, so the rectangle X and Y match
            _cursorX -= g.Advance;

            // clear where the old character was
            var fbSlice = _memory.Slice(_cursorX + pb.X + (CursorYMemory(_currentLine) + pb.Y) * _framebuffer.Width);
            for (int i = 0; i < pb.Height; i++) fbSlice.Slice(i * _framebuffer.Width, pb.Width).Span.Fill(0);
            BlitHelper(_cursorX + pb.X, CursorYMemory(_currentLine) + pb.Y, _cursorX + pb.X, CursorYScreen(_currentLine) + pb.Y, pb.Width, pb.Height);

            _framebuffer.Flush();
        }
    }

    public void InsertChar(char c)
    {
        _textBuffer[_currentLine].Add(c);

        var g = _font.Glyphs[(int)c - _font.First];
        var pb = g.PlaneBounds;
        _fontBlitter.DrawChar(c, _cursorX, CursorYMemory(_currentLine));

        BlitHelper(_cursorX + pb.X, CursorYMemory(_currentLine) + pb.Y, _cursorX + pb.X, CursorYScreen(_currentLine) + pb.Y, pb.Width, pb.Height);
        _framebuffer.Flush();

        _cursorX += g.Advance;
    }

    public void InsertLine(string str)
    {
        // NOTE: X coords are absolute, Y coords relative to line start
        int minX = Int32.MaxValue, minY = Int32.MaxValue;
        int maxX = Int32.MinValue, maxY = Int32.MinValue;
        foreach (var c in str)
        {
            _textBuffer[_currentLine].Add(c);

            var g = _font.Glyphs[(int)c - _font.First];
            var pb = g.PlaneBounds;
            _fontBlitter.DrawChar(c, _cursorX, CursorYMemory(_currentLine));

            minX = Math.Min(minX, _cursorX + pb.X);
            minY = Math.Min(minY, pb.Y);
            maxX = Math.Max(maxX, _cursorX + pb.X + pb.Width);
            maxY = Math.Max(maxY, pb.Y + pb.Height);

            _cursorX += g.Advance;
        }
        BlitHelper(minX, CursorYMemory(_currentLine) + minY, minX, CursorYScreen(_currentLine) + minY, maxX - minX, maxY - minY);
        _framebuffer.Flush();
        // newline
        _textBuffer.Add(new List<char>());
        _currentLine++;
        _cursorX = 0;
    }

    private void KeyboardHandler(in KeyEvent k)
    {
        switch (k.Released)
        {
            // handle shift/alt 
            case false when k.Code is KeyCode.LeftShift or KeyCode.RightShift: _shift++; return;
            case true when k.Code is KeyCode.LeftShift or KeyCode.RightShift: _shift--; return;
            case false when (k.Code == KeyCode.RightAlt): _altgr++; return;
            case true when (k.Code == KeyCode.RightAlt): _altgr--; return;
            
            // we got released
            case true:
                return;
            
            // we got pressed
            default:
                switch (k.Code)
                {
                    case KeyCode.Enter:
                        InsertNewLine();
                        break;
            
                    case KeyCode.Backspace:
                        InsertBackspace();
                        break;
            
                    default:
                    {
                        // get codepoint, with all the appropriate checks
                        var c = KeyMap.GetCodepoint(k.Code, _shift > 0, _altgr > 0);
                        if (c < _font.First || c >= (_font.First + _font.Glyphs.Length)) 
                            return;

                        InsertChar((char)c);
                        break;
                    }
                }

                break;
        }
    }
    public Terminal(IFramebuffer fb, Memory<uint> m, IKeyboard kbd, Font f)
    {
        _font = f;
        _memory = m;
        _framebuffer = fb;

        // the font blitter
        _fontBlitter = new FontBlitter(_font, _memory, _framebuffer.Width, _framebuffer.Height, 0xFFFFFFFF);
        _textBuffer.Add(new List<char>());

        _maxLines = _framebuffer.Height / _font.Size;

        // register the keyboard callback
        // kbd.RegisterCallback(KeyboardHandler);
    }
    
}