using System;
using System.Collections.Generic;
using System.Drawing;
using Pentagon.Interfaces;

namespace Pentagon.Drivers.Graphics.Plain;

internal class PlainFramebuffer : IFramebuffer
{

    internal readonly List<PlainGraphicsOutput> Outputs = new();
    
    private Memory<byte> _memory;
    private int _width;

    private Memory<byte> _backing = Memory<byte>.Empty;

    public PlainFramebuffer(int width, int height)
    {
        _width = width;
        _memory = new Memory<byte>(new byte[width * height * 4]);
    }
    
    public Memory<byte> Backing
    {
        set => _backing = value;
    }

    public void Blit(int offset, in Rectangle rectangle)
    {

        if (rectangle.X == 0 && _width == rectangle.Width)
        {
            // a single copy can be used
            var src = _backing.Span.Slice(offset, rectangle.Width * rectangle.Height * 4);
            var dst = _memory.Span.Slice(rectangle.Y * _width * 4);
            src.CopyTo(dst);
        }
        else
        {
            // need to do multiple iterations 
            var src = _backing.Span.Slice(offset);
            var dst = _memory.Span.Slice((rectangle.X + rectangle.Y * _width) * 4);
            var bytesPerLine = 4 * rectangle.Width;

            for (
                var srcY = 0; srcY < rectangle.Height; srcY++,
                src = src.Slice(bytesPerLine), 
                dst = dst.Slice(_width)
            )
            {
                src.Slice(0, bytesPerLine).CopyTo(dst);
            }
        }
        
    }

    public void Flush()
    {
        foreach (var output in Outputs)
        {
            // we are always going to copy the entire range 
            var src = _memory.Span;
            
            var rect = output.Rectangle;
            if (rect.X == 0 && _width * 4 == output.RowBytes)
            {
                // fast blit path
                var dst = output.Address.Span.Slice(rect.Y * _width * 4);
                src.CopyTo(dst);
            }
            else
            {
                // need to do multiple iterations 
                var dst = output.Address.Span.Slice(rect.X + rect.Y * output.RowBytes);
                var bytesPerLine = output.RowBytes;

                for (
                    var srcY = 0; srcY < rect.Height; srcY++,
                    src = src.Slice(bytesPerLine), 
                    dst = dst.Slice(_width)
                )
                {
                    src.Slice(0, bytesPerLine).CopyTo(dst);
                }
            }
        }
    }

    public void Dispose()
    {
        _memory = Memory<byte>.Empty;
    }
}
