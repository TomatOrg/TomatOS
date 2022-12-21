using System;
using System.Drawing;

namespace Tomato.Graphics;

public struct Glyph
{
    public float Advance { get; }
    public RectangleF PlaneBounds { get; }
    public RectangleF AtlasBounds { get; }

    public Glyph(float advance, in RectangleF planeBounds, in RectangleF atlasBounds)
    {
        Advance = advance;
        PlaneBounds = planeBounds;
        AtlasBounds = atlasBounds;
    }
    
    public System.Drawing.Rectangle GetIntegerPlaneBounds(float x, float y)
    {
        var planeX = x + PlaneBounds.X;
        var planeY = y + PlaneBounds.Y;

        int xStartPx = (int)(planeX + 1000) - 1000;
        int yStartPx = (int)(planeY + 1000) - 1000;
        int xEndPx = (int)(planeX + PlaneBounds.Width + 1 + 1000) - 1000;
        int yEndPx = (int)(planeY + PlaneBounds.Height + 1 + 1000) - 1000;
        
        return Rectangle.FromLTRB(xStartPx, yStartPx, xEndPx, yEndPx);
    }
}

public class Font
{

    // the raw info
    public int Size { get; }
    public Typeface Typeface { get; }
    
    // the calculated metrics
    public float LineHeight { get; }
    public float Ascender { get; }
    public float Descender { get; }
    public float UnderlineY { get; }
    public float UnderlineThickness { get; }
    
    // the first and last char
    public char First { get; }
    public char Last { get; }
    
    // the calculated glyphs
    public Glyph[] Glyphs { get; }

    public int AtlasWidth => Typeface.Atlas.Width;
    public Memory<uint> Pixels => Typeface.Pixels;

    public Font(Typeface typeface, int size)
    {
        Typeface = typeface;
        Size = size;

        First = typeface.Atlas.First;
        Last = typeface.Atlas.Last;

        // calculate the metrics
        LineHeight = typeface.Metrics.LineHeight * size;
        Ascender = typeface.Metrics.Ascender * size;
        Descender = typeface.Metrics.Descender * size;
        UnderlineY = typeface.Metrics.UnderlineY * size;
        UnderlineThickness = typeface.Metrics.UnderlineThickness * size;

        // calculate the glyphs
        var glyphs = typeface.Glyphs.Span;
        Glyphs = new Glyph[glyphs.Length];
        for (var i = 0; i < Glyphs.Length; i++)
        {
            ref var glyph = ref glyphs[i];
            var planeBounds = RectangleF.FromLTRB(
                glyph.PlaneBound.Left * size,
                glyph.PlaneBound.Top * size,
                glyph.PlaneBound.Right * size,
                glyph.PlaneBound.Bottom * size
            );
            var atlasBounds = RectangleF.FromLTRB(
                glyph.AtlasBound.Left,
                glyph.AtlasBound.Top,
                glyph.AtlasBound.Right,
                glyph.AtlasBound.Bottom
            );
            Glyphs[i] = new Glyph(glyph.Advance * size, planeBounds, atlasBounds);
        }
    }
    
}
