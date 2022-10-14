using System;
using System.Collections.Generic;
using System.Drawing;
using System.Runtime.InteropServices;
using Tomato.DriverServices;
using System.Threading;
using Tomato.Graphics;
using Tomato.Gui.Framework;
using Tomato.Interfaces;

namespace Tomato.Gui.Server;

/// <summary>
/// This is both a client and server implementation for apps running and displaying locally.
/// </summary>
public class LocalGuiServer : GuiServer
{

    private IFramebuffer _framebuffer;
    private IKeyboard _keyboard;
    private IRelMouse _mouse;

    private AutoResetEvent _reset = new(false);
    private GuiEvent _event = null;

    private Memory<uint> _memory;
    private Memory<uint> _memoryUnderMouse;
    private int _width, _height;

    private int _mouseX, _mouseY;
    private int _oldMouseX, _oldMouseY;
    private Dictionary<int, Font> _fonts = new();

    public LocalGuiServer(IFramebuffer framebuffer, IKeyboard keyboard, IRelMouse mouse)
    {
        _framebuffer = framebuffer;
        _keyboard = keyboard;
        _mouse = mouse;
        _width = framebuffer.Width;
        _height = framebuffer.Height;
        
        // allocate the framebuffer
        var memory = new byte[framebuffer.Width * framebuffer.Height * 4].AsMemory();
        var memoryUnderMouse = new byte[8 * 8 * 4].AsMemory();
        
        // set the backing 
        _framebuffer.Backing = memory;
        
        // keep it as a uint array for blitter
        _memory = MemoryMarshal.Cast<byte, uint>(memory);
        _memoryUnderMouse = MemoryMarshal.Cast<byte, uint>(memoryUnderMouse);

        _oldMouseX = _mouseX = _width / 2;
        _oldMouseY = _mouseY = _height / 2;

        BlitMouse();
        _framebuffer.Flush();

        _keyboard.RegisterCallback(KeyboardCallback);
        _mouse.RegisterCallback(MouseCallback);
    }
    
    void BlitMouse()
    {
        int oldStartX = Bound(_oldMouseX - 4, 0, _width - 1), oldStartY = Bound(_oldMouseY - 4, 0, _height - 1);
        int oldEndX = Bound(_oldMouseX + 4, 0, _width - 1), oldEndY = Bound(_oldMouseY + 4, 0, _height - 1);
        for (int i = 0; i < oldEndY - oldStartY; i++)
        {
            var under = _memoryUnderMouse.Span.Slice(8 * i, oldEndX - oldStartX);
            var fb = _memory.Span.Slice((oldStartY + i) * _width + oldStartX);
            under.CopyTo(fb);
        }
        int startX = Bound(_mouseX - 4, 0, _width - 1), startY = Bound(_mouseY - 4, 0, _height - 1);
        int endX = Bound(_mouseX + 4, 0, _width - 1), endY = Bound(_mouseY + 4, 0, _height - 1);
        for (int i = 0; i < endY - startY; i++)
        {
            var under = _memoryUnderMouse.Span.Slice(8 * i);
            var fb = _memory.Span.Slice((startY + i) * _width + startX, endX - startX);
            fb.CopyTo(under);
            fb.Fill(0xFFFFFFFF);
        }
        // blit the backing to the framebuffer
        int sx = Math.Min(oldStartX, startX), sy = Math.Min(oldStartY, startY);
        int ex = Math.Max(oldEndX, endX), ey = Math.Max(oldEndY, endY);
        _framebuffer.Blit(sy * _width + sx, new Rectangle(sx, sy, ex - sx, ey - sy));
        _oldMouseX = _mouseX;
        _oldMouseY = _mouseY;
    }

    int Bound(int val, int start, int end) => Math.Max(start, Math.Min(val, end));

    void MouseCallback(RelMouseEvent e)
    {
        _mouseX += e.deltaX;
        _mouseY += e.deltaY;

        if (_oldMouseX != _mouseX || _oldMouseY != _mouseY)
        {
            BlitMouse();
            _framebuffer.Flush();
        }

        _event = e;
        _reset.Set();
    }
    
    void KeyboardCallback(KeyEvent e)
    {
        _event = e;
        _reset.Set();
    }
    
    // TODO: add support for compiling expressions, it will
    //       make the code much faster :)
    private long Eval(Expr e)
    {
        switch (e.Type)
        {
            case ExprType.IntLiteral: 
                return ((IntLiteralExpr)e).Value;
            
            case ExprType.InfLiteral:
                return long.MaxValue;
            
            case ExprType.Var:
            {
                var node = (VarExpr)e;
                // TODO: type checking
                return node.Name switch
                {
                    "width" => _width,
                    "height" => _height,
                    _ => throw new InvalidOperationException($"unknown gui variable {node.Name}")
                };
            }

            case ExprType.Add: { var add = (BinaryExpr)e; var a = Eval(add.A); var b = Eval(add.B); if (a == long.MaxValue || b == long.MaxValue) return long.MaxValue; return a + b; }
            case ExprType.Mul: { var add = (BinaryExpr)e; var a = Eval(add.A); var b = Eval(add.B); if (a == long.MaxValue || b == long.MaxValue) return long.MaxValue; return a * b; }
            case ExprType.Div: { var add = (BinaryExpr)e; var a = Eval(add.A); var b = Eval(add.B); if (a == long.MaxValue || b == long.MaxValue) return long.MaxValue; return a / b; }
            case ExprType.Mod: { var add = (BinaryExpr)e; var a = Eval(add.A); var b = Eval(add.B); if (a == long.MaxValue || b == long.MaxValue) return long.MaxValue; return a % b; }
            // case ExprType.Pow: { var add = (BinaryExpr)e; return Eval(add.A) ** Eval(add.B); }
            case ExprType.Eq: { var add = (BinaryExpr)e; return Eval(add.A) == Eval(add.B) ? 1 : 0; }
            case ExprType.Neq: { var add = (BinaryExpr)e; return Eval(add.A) != Eval(add.B) ? 1 : 0; }
            case ExprType.Lt: { var add = (BinaryExpr)e; return Eval(add.A) < Eval(add.B) ? 1 : 0; }
            case ExprType.Lte: { var add = (BinaryExpr)e; return Eval(add.A) <= Eval(add.B) ? 1 : 0; }
            case ExprType.Gt: { var add = (BinaryExpr)e; return Eval(add.A) > Eval(add.B) ? 1 : 0; }
            case ExprType.Gte: { var add = (BinaryExpr)e; return Eval(add.A) >= Eval(add.B) ? 1 : 0; }
            case ExprType.BAnd: { var add = (BinaryExpr)e; return Eval(add.A) & Eval(add.B); }
            case ExprType.BOr: { var add = (BinaryExpr)e; return Eval(add.A) | Eval(add.B); }
            case ExprType.Neg: { var add = (UnaryExpr)e; return -Eval(add.A); }
            case ExprType.BInvert: { var add = (UnaryExpr)e; return ~Eval(add.A); }
            case ExprType.Min: { var add = (BinaryExpr)e; return Math.Min(Eval(add.A), Eval(add.B)); }
            case ExprType.Max: { var add = (BinaryExpr)e; return Math.Max(Eval(add.A), Eval(add.B)); }
            case ExprType.Abs: { var add = (UnaryExpr)e; return Math.Abs(Eval(add.A)); }

            case ExprType.If:
                var @if = (IfExpr)e;
                return Eval(Eval(@if.Condition) != 0 ? @if.True : @if.False);

            case ExprType.MeasureTextX:
                var measureTextX = (MeasureTextXExpr)e;
                return MeasureTextX(measureTextX.Text, (int)Eval(measureTextX.FontSize));
            
            case ExprType.MeasureTextY:
                var measureTextY = (MeasureTextYExpr)e;
                var font = GetFont((int)Eval(measureTextY.FontSize));
                return font.Size;
            
            default:
                throw new InvalidOperationException("Invalid gui expression type");
        }
    }

    private Font GetFont(int fontSize)
    {
        if (!_fonts.ContainsKey(fontSize))
        {
            _fonts[fontSize] = new Font(Typeface.Default, fontSize);
        }
        return _fonts[fontSize];
    }
    
    private long MeasureTextX(string text, int fontSize)
    {
        var font = GetFont(fontSize);
        var length = 0;
        foreach (var c in text)
        {
            if (c < font.First || c > font.Last)
                continue;
            length += font.Glyphs[c - font.First].Advance;
        }
        return length;
    }

    public override void Accept()
    {
        // nothing to do in here
    }

    public override void Handle()
    {
        // TODO: handle inputs in here and push them to the event queue
        //       for the GetEvent to handle
        while (true)
        {
            _reset.WaitOne();
            EventHandler.Invoke(_event);
        }
    }

    public override void UpdateScene(Scene scene)
    {
        // TODO: use an oplist instead of a plain expression, that can be 
        //       used to calculate common expressions just once 
        
        foreach (var command in scene.Commands)
        {
            switch (command.CommandType)
            {
                case CommandType.Clear:
                {
                    var cmd = (ClearCommand)command;
                    var blitter = new Blitter(_memory, _width, (uint)Eval(cmd.Color));
                    blitter.BlitRect(0, 0, _framebuffer.Width, _height);
                } break;
                
                case CommandType.Rect:
                {
                    var cmd = (RectCommand)command;
                    var blitter = new Blitter(_memory, _width, (uint)Eval(cmd.Color));
                    var left = (int)Eval(cmd.Left);
                    var top = (int)Eval(cmd.Top);
                    var right = (int)Eval(cmd.Right);
                    var bottom = (int)Eval(cmd.Bottom);
                    blitter.BlitRect(left, top, unchecked(right - left), unchecked(bottom - top));
                } break;

                case CommandType.Text:
                {
                    var cmd = (TextCommand)command;
                    var fontSize = (int)Eval(cmd.FontSize);
                    var font = GetFont(fontSize);
                    var blitter = new FontBlitter(font, _memory, _width, _height, (uint)Eval(cmd.Color));
                    var measurement = MeasureTextX(cmd.Text, fontSize);
                    var x = Eval(cmd.X);
                    var y = Eval(cmd.Y);
                    blitter.DrawString(cmd.Text, (int)(x - measurement / 2), (int)(y + font.Size / 2));
                } break;
                
                default:
                    throw new InvalidOperationException("Invalid gui command type");
            }
        }

        int startX = Bound(_mouseX - 4, 0, _width - 1), startY = Bound(_mouseY - 4, 0, _height - 1);
        int endX = Bound(_mouseX + 4, 0, _width - 1), endY = Bound(_mouseY + 4, 0, _height - 1);
        for (int i = 0; i < endY - startY; i++)
        {
            var under = _memoryUnderMouse.Span.Slice(8 * i);
            var fb = _memory.Span.Slice((startY + i) * _width + startX, endX - startX);
            fb.CopyTo(under);
        }

        // blit the backing to the framebuffer
        _framebuffer.Blit(0, new Rectangle(0, 0, _width, _height));
        BlitMouse();
        _framebuffer.Flush();
    }

}