/*
 * Code translated from Skia
 * 
 * Copyright 2006 The Android Open Source Project
 *
 * Copyright (c) 2011 Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 * 
 *   * Neither the name of the copyright holder nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
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
    public void BlitPixel(int x, int y)
    {
        ref var dst = ref _memory.Span[x + y * _width];
        var color = _pmColor;
        
        switch (_pmColor >> 24)
        {
            case 0: break;
            case 255: dst = color; break;
            default: 
                var invA = 255 - (color >> 24);
                invA += invA >> 7;
                dst = dst * invA + (((color << 8) + 128) >> 8);
                break;
        }
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
            case 0: break;
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
