using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Tomato.Hal;

public class Field<T> 
    where T : unmanaged
{

    // keep reference to the region
    private Region _region;
    private readonly ulong _fieldPtr;

    public ref T Value => ref MemoryServices.UnsafePtrToRef<T>(_fieldPtr);
    
    internal Field(Region region, int offset)
    {
        _region = region;
        
        // slice the span to get into the correct offset and then get the raw ptr
        var span = region.Span.Slice(offset, Unsafe.SizeOf<T>());
        _fieldPtr = MemoryServices.GetSpanPtr(ref span);
    }

}

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

    public Memory<T> CreateMemory<T>(int offset, int count)
        where T : unmanaged
    {
        var sliced = _memory.Slice(offset, count * Unsafe.SizeOf<T>());
        return MemoryMarshal.Cast<byte, T>(sliced);
    }

    public Memory<T> CreateMemory<T>(int offset)
        where T : unmanaged
    {
        var sliced = _memory.Slice(offset);
        return MemoryMarshal.Cast<byte, T>(sliced);
    }

    public Region CreateRegion(int offset, int size)
    {
        return new Region(_memory.Slice(offset, size));
    }

    public Region CreateRegion(int offset)
    {
        return new Region(_memory.Slice(offset));
    }

    public Span<T> AsSpan<T>(int offset, int count)
        where T : unmanaged
    {
        var sliced = _memory.Slice(offset, count * Unsafe.SizeOf<T>());
        return MemoryMarshal.Cast<byte, T>(sliced.Span);
    }
    
}