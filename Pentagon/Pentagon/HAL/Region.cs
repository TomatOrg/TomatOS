using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Pentagon.HAL;

public class Region
{

    private Memory<byte> _memory;

    public Memory<byte> Memory => _memory;
    public Span<byte> Span => _memory.Span;

    public Region(Memory<byte> memory)
    {
        _memory = memory;
    }

    public Field<T> CreateField<T>(int offset)
        where T : unmanaged
    {
        return new Field<T>(this, offset);
    }

    public Span<T> AsSpan<T>(int offset, int count)
        where T : unmanaged
    {
        var sliced = _memory.Slice(offset, count * Unsafe.SizeOf<T>());
        return MemoryMarshal.Cast<byte, T>(sliced.Span);
    }
    
}