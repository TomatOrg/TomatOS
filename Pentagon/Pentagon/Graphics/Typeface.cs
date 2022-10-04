using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Pentagon.DriverServices;

namespace Pentagon.Graphics;

[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct TypefaceAtlas
{
    public int DistanceRange { get; }
    public float Size { get; }
    public int Width { get; }
    public int Height { get; }
    public char First { get; }
    public char Last { get; }
}

[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct TypefaceMetrics
{
    public float LineHeight { get; }
    public float Ascender { get; }
    public float Descender { get; }
    public float UnderlineY { get; }
    public float UnderlineThickness { get; }
}
    
[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct TypefaceBounds
{
    public float Left { get; }
    public float Top { get; }
    public float Right { get; }
    public float Bottom { get; }
}

[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct TypefaceGlyph
{
    public float Advance { get; }
    public TypefaceBounds PlaneBound { get; }
    public TypefaceBounds AtlasBound { get; }
}

public class Typeface
{

    public static Typeface Default { get; private set; }

    public static void Load()
    {
        KernelUtils.GetDefaultFont(out var addr, out var size);
        var data = MemoryServices.Map(addr, size);
        Default = new Typeface(data);
    }
    
    public static void Load(Memory<byte> data)
    {
        Default = new Typeface(data);
    }
    
    internal TypefaceAtlas Atlas { get; }
    internal TypefaceMetrics Metrics { get; }
    internal Memory<TypefaceGlyph> Glyphs { get; }
    internal Memory<uint> Pixels { get; }

    public Typeface(Memory<byte> data)
    {
        var span = data.Span;
        Atlas = MemoryMarshal.Read<TypefaceAtlas>(span);
        Metrics = MemoryMarshal.Read<TypefaceMetrics>(span.Slice(Unsafe.SizeOf<TypefaceAtlas>()));

        // get the glyph slice 
        var glyphsOffset = Unsafe.SizeOf<TypefaceAtlas>() + Unsafe.SizeOf<TypefaceMetrics>();
        var glyphsSize = (Atlas.Last - Atlas.First + 1) * Unsafe.SizeOf<TypefaceGlyph>();
        Glyphs = MemoryMarshal.Cast<byte, TypefaceGlyph>(data.Slice(glyphsOffset, glyphsSize));

        // get the pixels slice
        Pixels = MemoryMarshal.Cast<byte, uint>(data.Slice(glyphsOffset + glyphsSize, Atlas.Width * Atlas.Height * 4));
    }
    
}