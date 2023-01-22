using System;
using System.Collections.Generic;
using System.Threading;
using System.Drawing;
using System.Diagnostics;
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
    private float _cursorX;
    // Rounded font size
    private int _fontSize;

    // Keyboard state
    private int _shift = 0, _altgr = 0;

    public struct Cell
    {
        public float X;
        public int Line;
        public char Char;
        public bool Delete;

        public Cell(char c, float x, int l, bool del)
        {
            Char = c;
            X = x;
            Line = l;
            Delete = del;
        }
    }
    private List<Cell> _deferredList = new();
    private int _deferredScroll = 0;

    // blit backbuffer position (mx,my) to frontbuffer (sx,sy)
    private void BlitHelper(int mx, int my, int sx, int sy, int w, int h)
    {
        _framebuffer.Blit(mx + my * _framebuffer.Width, new Rectangle(sx, sy, w, h));
    }

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
    // Note that the Y needs to account for the descender (we pass the cursor position to the FontBlitter, not top left), otherwise characters get cut off
    public int TopYMemory(int line) => (line % _maxLines) * _fontSize;
    public int TopYScreen(int line) => (line - _firstLine) * _fontSize;
    public int CursorYMemory(int line) => TopYMemory(line) + 14;
    private int CursorYScreen(int line) => TopYScreen(line) + 14;

    public void Scroll(int scrollBy)
    {
        lock (_deferredList) _deferredScroll += scrollBy;
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

            // get information on the popped glyph.
            var g = _font.Glyphs[ch - _font.First];

            // pb represents the character to delete, so move the cursor back to where it was when that character was added, so the rectangle X and Y match
            _cursorX -= g.Advance;
            // get information on where it was
            var pb = g.GetIntegerPlaneBounds(_cursorX, 0);

            // clear where the old character was
            lock (_deferredList) _deferredList.Add(new Cell(ch, _cursorX, _currentLine, true));
        }
    }

    public void InsertChar(char c)
    {
        if (c < _font.First || c >= _font.Last) return;
        _textBuffer[_currentLine].Add(c);
        var g = _font.Glyphs[(int)c - _font.First];
        lock (_deferredList) _deferredList.Add(new Cell(c, _cursorX, _currentLine, false));
        _cursorX += g.Advance;
    }

    public void Insert(string str)
    {
        foreach (var c in str) InsertChar(c);
    }

    List<char> _readlineBuffer;
    AutoResetEvent _readlineAre = new AutoResetEvent(false);

    private void KeyboardHandler(in KeyEvent k)
    {
        switch (k.Released)
        {
            // handle shift/alt 
            case false when k.Code is KeyMap.LeftShift or KeyMap.RightShift: _shift++; return;
            case true when k.Code is KeyMap.LeftShift or KeyMap.RightShift: _shift--; return;
            case false when (k.Code == KeyMap.RightAlt): _altgr++; return;
            case true when (k.Code == KeyMap.RightAlt): _altgr--; return;
            // we got released
            case true: return;
            // we got pressed
            default: KeyPress(k.Code); return;
        }

        void KeyPress(int k)
        {
            switch (k)
            {
                case KeyMap.Enter:
                    if (_readlineAre != null) _readlineAre.Set();
                    InsertNewLine();
                    break;

                case KeyMap.Backspace:
                    if (_readlineBuffer != null) _readlineBuffer.RemoveAt(_readlineBuffer.Count - 1);
                    InsertBackspace();
                    break;

                default:
                    // get codepoint, with all the appropriate checks
                    var c = KeyMap.GetCodepoint(k, _shift > 0, _altgr > 0);
                    if (c < _font.First || c >= (_font.First + _font.Glyphs.Length))
                        return;
                    if (_readlineBuffer != null) _readlineBuffer.Add((char)c);
                    InsertChar((char)c);
                    break;
            }
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

        _fontSize = (int)(-_font.Ascender + 0.99);

        _maxLines = _framebuffer.Height / _fontSize;

        // register the keyboard callback
        kbd.Callback = KeyboardHandler;

        (new Thread(DeferredWriter)).Start();
    }

    public void DeferredScroll(int scrollBy)
    {
        _firstLine += scrollBy;
        if (scrollBy >= _maxLines)
        {
            for (var i = 0; i < _framebuffer.Height; i++) _memory.Slice(i * _framebuffer.Width, _framebuffer.Width).Span.Fill(0);
            BlitHelper(0, 0, 0, 0, _framebuffer.Width, _framebuffer.Height);
            return;
        }

        var screenLine = 0;

        for (var j = 0; j < scrollBy; j++)
        {
            var m = _memory[(((_firstLine - scrollBy + j) % _maxLines) * _fontSize * _framebuffer.Width)..];
            for (var i = 0; i < _fontSize; i++) m.Slice(i * _framebuffer.Width, _framebuffer.Width).Span.Fill(0);
        }

        Blit(_firstLine % _maxLines, _maxLines - (_firstLine % _maxLines));
        Blit(0, _firstLine % _maxLines);


        void Blit(int memY, int lines)
        {
            BlitHelper(0, memY * _fontSize, 0, screenLine * _fontSize, _framebuffer.Width, lines * _fontSize);
            screenLine += lines;
        }
    }

    public void DeferredWriter()
    {
        var stopwatch = new Stopwatch();
        while (true)
        {
            stopwatch.Reset();
            lock (_deferredList)
            {
                if (_deferredScroll != 0)
                {
                    DeferredScroll(_deferredScroll);
                    _deferredScroll = 0;
                }
                foreach (var cell in _deferredList)
                {
                    if (TopYScreen(cell.Line) < 0) continue;
                    var g = _font.Glyphs[(int)cell.Char - _font.First];
                    var pb = g.GetIntegerPlaneBounds(cell.X, 0);

                    if (!cell.Delete) _fontBlitter.DrawChar(cell.Char, cell.X, CursorYMemory(cell.Line));
                    else
                    {
                        var fbSlice = _memory.Slice(pb.X + (CursorYMemory(_currentLine) + pb.Y) * _framebuffer.Width);
                        for (int i = 0; i < pb.Height; i++) fbSlice.Slice(i * _framebuffer.Width, pb.Width).Span.Fill(0);
                    }

                    BlitHelper(pb.X, CursorYMemory(cell.Line) + pb.Y,
                        pb.X, CursorYScreen(cell.Line) + pb.Y,
                        pb.Width, pb.Height);
                }
                _deferredList.Clear();
            }
            _framebuffer.Flush();
            
            var time = stopwatch.ElapsedMilliseconds;
            var timeInFrame = time % 16;
            var sleepFor = 16 - timeInFrame;
            if (sleepFor < 8) sleepFor += 16; // wait a frame more (halving framerate) if we used more than 50% of frame time
            // Debug.Write($"{time}\n");
            Thread.Sleep((int)sleepFor);
        }
    }

    public string ReadLine()
    {
        _readlineBuffer = new List<char>();
        _readlineAre.WaitOne();
        var str = new string(_readlineBuffer.ToArray());
        _readlineBuffer = null;
        return str;
    }
}
