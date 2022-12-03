using System;
using System.Diagnostics;
using System.Runtime.CompilerServices;

namespace Tomato.Graphics;

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

        float screenPxRange = (font.Size / font.Typeface.Atlas.Size) * font.Typeface.Atlas.DistanceRange;
        _screenPxRange = Math.Max(256, (int)(screenPxRange * 256));
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
    private void DrawInternal(ReadOnlySpan<uint> pixels, Span<uint> memory, in Glyph glyph, float x, float y, uint color)
    {
        var fullAtlasWidth = _font.AtlasWidth;
        
        // Get the stored glyph rectangle and the correctly rounded pixel one
        var ab = glyph.AtlasBounds;
        var pbInt = glyph.GetIntegerPlaneBounds(x, y);
        var pb = glyph.PlaneBounds;
        pb.X += x;
        pb.Y += y;
        
        // How much should we increment the atlas for every screen space pixel?
        int xDeltaFix = (int)((ab.Width / pb.Width) * 256);
        int yDeltaFix = (int)((ab.Height / pb.Height) * 256);
        
        // Get the distance between the pixel starting position 
        // and the real position where it should've started
        // (all in screen space coordinates)
        int xEccessFix = (int)((pb.X - pbInt.X) * 256);
        int yEccessFix = (int)((pb.Y - pbInt.Y) * 256);
        
        // Convert EccessFix into an atlas-space coordinate
        // the +0.5 (+128) is required for point sampling
        // (which the generated MSDF atlas assumes)  
        int xFractionalStartFix = (int)((-xEccessFix + 128) * xDeltaFix / 256 + 128); 
        int yFractionalStartFix = (int)((-yEccessFix + 128) * yDeltaFix / 256 + 128); 

        // If it starts below zero, fix by starting at 0
        if (pbInt.X < 0)
        {
            xFractionalStartFix += -pbInt.X * 256;
            pbInt.Width += pbInt.X;
            pbInt.X = 0;
        }
        if (pbInt.Y < 0)
        {
            yFractionalStartFix += -pbInt.Y * 256;
            pbInt.Height += pbInt.Y;
            pbInt.Y = 0;
        }
        
        // pixels[CurrAtlasX + CurrAtlasY * fullAtlasWidth] has a constant index of
        // StartAtlasX + StartAtlasY * fullAtlasWidth, which we can calculate only once
        var atlas = pixels.Slice((int)ab.X + (int)ab.Y * fullAtlasWidth);
        
        for (int yy = 0; yy < pbInt.Height; yy++)
        {
            // The current Y coordinate inside the atlas
            int yFix = yFractionalStartFix + yy * yDeltaFix;
            
            // Its integer and fractional coordinate
            int yPx = yFix >> 8, yfFix = yFix & 0xFF;
            
            // Offset inside the line
            int off = yPx * fullAtlasWidth;
            
            // Check if the current and next line are inside bounds
            // If not, the inner loop will put 0
            bool yCheck = (yPx >= 0) && (yPx < ab.Height);
            bool y1Check = ((yPx + 1) >= 0) && ((yPx + 1) < ab.Height);

            for (int xx = 0; xx < pbInt.Width; xx++)
            {
                int xFix = xFractionalStartFix + xx * xDeltaFix;
                int xPx = xFix >> 8, xfFix = xFix & 0xFF;
                
                // 1 - fractional part, for bilinear interpolation
                int xnFix = 255 - xfFix, ynFix = 255 - yfFix;
                
                // Inaccurate error check
                if (xPx < 0 || (xPx+1) >= ab.Width) continue;

                // Get the RGB components of the four pixels
                uint p00 = yCheck ? atlas[off + xPx] : 0;
                uint p10 = yCheck ? atlas[off + xPx + 1] : 0;
                uint p01 = y1Check ? atlas[off + fullAtlasWidth + xPx] : 0;
                uint p11 = y1Check ? atlas[off + fullAtlasWidth + xPx + 1] : 0;
                int p00r = (int)(p00 & 0xFF), p00g = (int)((p00 >> 8) & 0xFF), p00b = (int)((p00 >> 16) & 0xFF);                
                int p10r = (int)(p10 & 0xFF), p10g = (int)((p10 >> 8) & 0xFF), p10b = (int)((p10 >> 16) & 0xFF);
                int p01r = (int)(p01 & 0xFF), p01g = (int)((p01 >> 8) & 0xFF), p01b = (int)((p01 >> 16) & 0xFF);
                int p11r = (int)(p11 & 0xFF), p11g = (int)((p11 >> 8) & 0xFF), p11b = (int)((p11 >> 16) & 0xFF);
        
                // Unoptimized linear interpolation
                int r = ((p00r * xnFix + p10r * xfFix) / 256 * ynFix / 256) + ((p01r * xnFix + p11r * xfFix) / 256 * yfFix / 256);
                int g = ((p00g * xnFix + p10g * xfFix) / 256 * ynFix / 256) + ((p01g * xnFix + p11g * xfFix) / 256 * yfFix / 256);
                int b = ((p00b * xnFix + p10b * xfFix) / 256 * ynFix / 256) + ((p01b * xnFix + p11b * xfFix) / 256 * yfFix / 256);

                // Compute the median
                int minRG = (r < g) ? r : g;
                int maxRG = (r > g) ? r : g;
                int a = (maxRG < b) ? maxRG : b;
                a = (minRG > a) ? minRG : a;

                // Convert it to a screen distance
                uint val = (uint)(_screenPxRange * (a - 128) / 256 + 128);
                
                memory[(pbInt.X + xx) + (pbInt.Y + yy) * _width] = val | (val << 8) | (val << 16);
            }
        }
    }
    
    public void DrawChar(char c, float cursorX, float cursorY)
    {
        var pixels = _font.Pixels.Span;
        var memory = _memory.Span;
        var color = _color;
        
        // filter invalid chars
        if (c < _font.First || c > _font.Last)
            return;

        // filter empty bitmaps
        ref var glyph = ref _font.Glyphs[c - _font.First];

        // draw it  
        if (glyph.AtlasBounds.Width != 0)
        {
            DrawInternal(pixels, memory, glyph, cursorX, cursorY, color);
        }
    }
}
