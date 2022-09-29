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
    private float _screenPxRange;

    public FontBlitter(Font font, Memory<uint> memory, int width, int height, uint color)
    {
        _memory = memory;
        _width = width;
        _height = height;
        _font = font;
        _color = color;

        _screenPxRange = (font.Size / font.Typeface.Atlas.Size) * font.Typeface.Atlas.DistanceRange;
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private static byte Median(byte r, byte g, byte b)
    {
        return Math.Max(Math.Min(r, g), Math.Min(Math.Max(r, g), b));
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private static float Clamp(float value, float min, float max)
    {
        if (value < min)
        {
            return min;
        }
        return value > max ? max : value;
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private static uint Mix(uint back, uint fore, float opacity)
    {
        var br = (byte)back;
        var bg = (byte)(back >> 8);
        var bb = (byte)(back >> 16);
            
        var fr = (byte)fore;
        var fg = (byte)(fore >> 8);
        var fb = (byte)(fore >> 16);

        var r = (byte)(fr * opacity + br * (1.0f - opacity));
        var g = (byte)(fg * opacity + bg * (1.0f - opacity));
        var b = (byte)(fb * opacity + bb * (1.0f - opacity));

        return r | ((uint)g << 8) | ((uint)b << 16);
    }
    
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private uint ProcessPixel(uint mtsdf, uint bg, uint fg)
    {
        var r = (byte)mtsdf;
        var g = (byte)(mtsdf >> 8);
        var b = (byte)(mtsdf >> 16);
        var sd = Median(r, g, b) / 255.0f;
        var screenPxDistance = _screenPxRange * (sd - 0.5f);
        var opacity = Clamp(screenPxDistance + 0.5f, 0.0f, 1.0f);
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
                var fax = (xx / (float)planeWidth) * atlasWidth;
                var fay = (yy / (float)planeHeight) * atlasHeight;

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