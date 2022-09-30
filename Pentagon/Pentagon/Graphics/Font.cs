using System;
using System.Drawing;

namespace Pentagon.Graphics;

public struct Glyph
{
    public int Advance { get; }
    public Rectangle PlaneBounds { get; }
    public RectangleF AtlasBounds { get; }

    public Glyph(int advance, in Rectangle planeBounds, in RectangleF atlasBounds)
    {
        Advance = advance;
        PlaneBounds = planeBounds;
        AtlasBounds = atlasBounds;
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

    public int AtalaWidth => Typeface.Atlas.Width;
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
            var planeBounds = Rectangle.FromLTRB(
                (int)(glyph.PlaneBound.Left * size),
                (int)(glyph.PlaneBound.Top * size),
                (int)(glyph.PlaneBound.Right * size),
                (int)(glyph.PlaneBound.Bottom * size)
            );
            var atlasBounds = RectangleF.FromLTRB(
                (int)(glyph.AtlasBound.Left),
                (int)(glyph.AtlasBound.Top),
                (int)(glyph.AtlasBound.Right),
                (int)glyph.AtlasBound.Bottom
            );
            Glyphs[i] = new Glyph((int)(glyph.Advance * size), planeBounds, atlasBounds);
        }
    }
    
}