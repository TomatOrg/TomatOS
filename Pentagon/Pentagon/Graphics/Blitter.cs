/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

using System;
using System.Runtime.CompilerServices;

namespace Pentagon.Graphics;

public struct Blitter
{

    private Memory<uint> _memory;
    private int _width;
    private uint _pmColor;
    private uint _srcA;

    public Blitter(Memory<uint> memory, int width, uint color)
    {
        _memory = memory;
        _width = width;

        _srcA = color >> 24;
        var scale = _srcA + 1;
        var srcR = (((color >> 16) & 0xFF) * scale) >> 8;
        var srcG = (((color >> 8) & 0xFF) * scale) >> 8;
        var srcB = ((color & 0xFF) * scale) >> 8;

        _pmColor = (_srcA << 24) | (srcR << 16) | (srcG << 8) | srcB;
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private static void BlitBlendRow(Span<uint> dst, Span<uint> src, uint color)
    {
        var invA = 255 - (color >> 24);
        invA += invA >> 7;
        
        for (var i = 0; i < dst.Length; i++)
        {
            dst[i] = src[i] * invA + (((color << 8) + 128) >> 8);
        }
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private static void BlitRow(Span<uint> dst, Span<uint> src, uint color)
    {
        switch (color >> 24)
        {
            case 0: src.CopyTo(dst);  break;
            case 255: dst.Fill(color); break;
            default: BlitBlendRow(dst, src, color); break;
        }            
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private static uint AlphaMulQ(uint c, uint scale)
    {
        const int mask = 0xFF00FF;
        var rb = ((c & mask) * scale) >> 8;
        var ag = ((c >> 8) & mask) * scale;
        return (uint)((rb & mask) | (ag & ~mask));
    }

    public void BlitHorizontal(int x, int y, int width)
    {
        var addr = _memory.Span.Slice(x + y * _width, width);
        BlitRow(addr, addr, _pmColor);
    }
    
    public void BlitVertical(int x, int y, int height, byte alpha)
    {
        if (alpha == 0 || _srcA == 0)
            return;

        var addr = _memory.Span.Slice(x + y * _width);
        var color = _pmColor;
        
        if (alpha != 255)
        {
            color = AlphaMulQ(color, (uint)alpha + 1);
        }

        var dstScale = (255 - (color >> 24)) + 1;
        var rowBytes = _width;
        while (--height >= 0)
        {
            addr[0] = color + AlphaMulQ(addr[0], dstScale);
            addr = addr.Slice(rowBytes);
        }
    }

    public void BlitRect(int x, int y, int width, int height)
    {
        if (_srcA == 0)
            return;

        var addr = _memory.Span.Slice(x + y * _width);
        var color = _pmColor;
        var rowPixels = _width;

        if ((_pmColor >> 24) == 0xFF)
        {
            while (height-- > 0)
            {
                addr.Slice(0, width).Fill(color);
                addr = addr.Slice(rowPixels);
            }
        }
        else
        {
            while (height-- > 0)
            {
                BlitRow(addr.Slice(0, width), addr, color);
                addr = addr.Slice(rowPixels);
            }
        }
    }
}
