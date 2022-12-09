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

        int xStartPx = (int)(planeX + 10000) - 10000;
        int yStartPx = (int)(planeY + 10000) - 10000;
        int xEndPx = (int)(planeX + PlaneBounds.Width + 0.99f + 10000) - 10000;
        int yEndPx = (int)(planeY + PlaneBounds.Height + 0.99f + 10000) - 10000;
        
        return Rectangle.FromLTRB(xStartPx, yStartPx, xEndPx, yEndPx);
    }
}

public class Font
{

    // the raw info
    public int Size { get; }
    public Typeface Typeface { get; }
    
    // the calculated metrics
    public int LineHeight { get; }
    public int Ascender { get; }
    public int Descender { get; }
    public int UnderlineY { get; }
    public int UnderlineThickness { get; }
    
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
        LineHeight = (int)(typeface.Metrics.LineHeight * size);
        Ascender = (int)(typeface.Metrics.Ascender * size);
        Descender = (int)(typeface.Metrics.Descender * size);
        UnderlineY = (int)(typeface.Metrics.UnderlineY * size);
        UnderlineThickness = (int)(typeface.Metrics.UnderlineThickness * size);

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
