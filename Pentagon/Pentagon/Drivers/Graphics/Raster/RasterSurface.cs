using System;
using System.Runtime.InteropServices;
using Pentagon.Interfaces;

namespace Pentagon.Drivers.Graphics.Raster;

public class RasterSurface : ISurface
{

    public int Width { get; }
    public int Height { get; }

    private int _rowBytes;
    private Memory<byte> _pixels;

    public ICanvas Canvas { get; }

    public RasterSurface(int width, int height, Memory<byte> pixels, int rowBytes)
    {
        Width = width;
        Height = height;
        _rowBytes = rowBytes;
        _pixels = pixels;
        
        Canvas = new RasterCanvas(width, height, pixels, rowBytes);
    }
    
    public void Draw(ICanvas canvas, int startX, int startY)
    {
        if (canvas is RasterCanvas c)
        {
            var pixels = MemoryMarshal.Cast<uint, byte>(c.Pixels);

            // fast copy in case that we overlap completely
            // TODO: more fast paths for this
            if (startX == 0 && startY == 0 && _rowBytes == c.RowBytes && Height == c.Height)
            {
                pixels.CopyTo(_pixels);
            }
            else
            {
                // copy line by line 
                var srcPixels = pixels.Span;
                var dstPixels = _pixels.Span.Slice(startX * 4 + startY * _rowBytes);
                
                var srcHeight = Math.Min(Height, c.Height - startY);
                var bytesPerLine = Math.Min(Width, c.Width - startX) * 4;
                for (
                    var srcY = 0; srcY < srcHeight; srcY++,
                    srcPixels = srcPixels.Slice(c.RowBytes), 
                    dstPixels = dstPixels.Slice(_rowBytes)
                )
                {
                    srcPixels.Slice(0, bytesPerLine).CopyTo(dstPixels);
                }
            }
        }
        else
        {
            // TODO: fallback on reading the pixels and copying them 
            throw new NotSupportedException();
        }
    }

    public ISurface CreateSurface(int width, int height)
    {
        if (Width == width && Height == height)
        {
            // allocate with the same row bytes to make sure fast drawing is possible
            return new RasterSurface(width, height, new byte[_rowBytes * height], _rowBytes);
        }
        else
        {
            return new RasterSurface(width, height, new byte[width * height * 4], width * 4);
        }
    }
}