using System;
using System.Drawing;
using System.Runtime.InteropServices;
using Pentagon.Interfaces;

namespace Pentagon.Drivers.Graphics.Raster;

public class RasterCanvas : ICanvas
{

    internal Memory<uint> Pixels;
    internal readonly int Width;
    internal readonly int Height;
    internal readonly int RowBytes;

    public RasterCanvas(int width, int height, Memory<byte> pixels, int rowBytes)
    {
        if ((rowBytes % 4) != 0)
            throw new ArgumentException();
        
        Width = width;
        Height = height;
        Pixels = MemoryMarshal.Cast<byte, uint>(pixels);
        RowBytes = rowBytes;
    }

    public void Clear(uint color)
    {
        Pixels.Span.Fill(color);
    }

    public void DrawLine(PointF x0, PointF x1, in ICanvas.Paint paint)
    {
        throw new NotImplementedException();
    }

    public void DrawRect(RectangleF rect, in ICanvas.Paint paint)
    {
        throw new NotImplementedException();
    }

    public void DrawCircle(PointF center, float radius, in ICanvas.Paint paint)
    {
        throw new NotImplementedException();
    }

    public void DrawRoundRect(RectangleF rect, float rx, float ry, in ICanvas.Paint paint)
    {
        throw new NotImplementedException();
    }

    public void DrawGlyph(int codePoint, float x, float y, in ICanvas.Paint paint)
    {
        throw new NotImplementedException();
    }
}