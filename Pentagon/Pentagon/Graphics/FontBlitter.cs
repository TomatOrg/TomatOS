using System;
using System.Drawing;
using System.Runtime.CompilerServices;
using Pentagon.DriverServices;

namespace Pentagon.Graphics;

public struct FontBlitter
{

    private Memory<uint> _memory;
    private int _width;
    private int _height;
    private uint _color;
    private Font _font;
    private int _screenPxRange;

    public FontBlitter(Font font, Memory<uint> memory, int width, int height, uint color)
    {
        _memory = memory;
        _width = width;
        _height = height;
        _font = font;
        _color = color;

        _screenPxRange = (font.Size * 256 / (int)font.Typeface.Atlas.Size) * font.Typeface.Atlas.DistanceRange;
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private static byte Median(byte r, byte g, byte b)
    {
        return Math.Max(Math.Min(r, g), Math.Min(Math.Max(r, g), b));
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private static int Clamp(int value, int min, int max)
    {
        if (value < min)
        {
            return min;
        }
        return value > max ? max : value;
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private static uint Mix(uint back, uint fore, int opacity)
    {
        var br = (byte)back;
        var bg = (byte)(back >> 8);
        var bb = (byte)(back >> 16);
            
        var fr = (byte)fore;
        var fg = (byte)(fore >> 8);
        var fb = (byte)(fore >> 16);

        var r = (fr * opacity + br * (256 - opacity)) / 256;
        var g = (fg * opacity + bg * (256 - opacity)) / 256;
        var b = (fb * opacity + bb * (256 - opacity)) / 256;

        return (uint)r | ((uint)g << 8) | ((uint)b << 16);
    }
    
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private uint ProcessPixel(uint mtsdf, uint bg, uint fg)
    {
        var r = (byte)(mtsdf >> 0);
        var g = (byte)(mtsdf >> 8);
        var b = (byte)(mtsdf >> 16);
        var sd = Median(r, g, b);
        var screenPxDistance = (_screenPxRange * (sd - 128)) / 256;
        var opacity = Clamp(screenPxDistance + 128, 0, 256);
        return Mix(bg, fg, opacity);
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private void DrawChar(Span<uint> pixels, Span<uint> memory, in Glyph glyph, int x, int y, uint color)
    {
        var fullAtlassWidth = _font.AtalaWidth;
        
        var planeHeight = glyph.PlaneBounds.Height;
        var planeWidth = glyph.PlaneBounds.Width;
        var planeX = x + glyph.PlaneBounds.X;
        var planeY = y + glyph.PlaneBounds.Y;

        var atlasWidth = glyph.AtlasBounds.Width;
        var atlasHeight = glyph.AtlasBounds.Height;
        var atlasX = glyph.AtlasBounds.X;
        var atlasY = glyph.AtlasBounds.Y;
        
        for (var yy = 0; yy < planeHeight; yy++)
        {
            for (var xx = 0; xx < planeWidth; xx++)
            {
                var fax = xx * atlasWidth / planeWidth;
                var fay = yy * atlasHeight / planeHeight;

                var atlasSampledX = (int)(atlasX + fax);
                var atlasSampledY = (int)(atlasY + fay);

                var pix = pixels[atlasSampledX + atlasSampledY * fullAtlassWidth];

                var px = (int)(planeX + xx);
                var py = (int)(planeY + yy);
                var bg = memory[px + py * _width];
                memory[px + py * _width] = ProcessPixel(pix, bg, color);
            }
        }
    }
    
    public void DrawString(string text, int cursorX, int cursorY)
    {
        var pixels = _font.Pixels.Span;
        var memory = _memory.Span;
        var color = _color;
        cursorY -= _font.Size;
        
        foreach (var c in text)
        {
            // filter invalid chars
            if (c < _font.First || c > _font.Last)
                continue;

            // filter empty bitmaps
            ref var glyph = ref _font.Glyphs[c - _font.First];
            if (glyph.Advance == 0)
                continue;

            // draw it  
            if (glyph.AtlasBounds.Width != 0)
            {
                DrawChar(pixels, memory, glyph, cursorX, cursorY, color);
            }
            
            // draw it 
            cursorX += glyph.Advance;
        }
    }
    
}