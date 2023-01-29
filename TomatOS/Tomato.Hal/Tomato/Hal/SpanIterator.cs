using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Tomato.Hal;

public ref struct SpanIterator
{

    private Span<byte> _buffer;

    public int Offset { get; private set; } = 0;

    public int Left => _buffer.Length;
    
    public SpanIterator(Span<byte> buffer)
    {
        _buffer = buffer;
    }

    public void Skip<T>(int count = 1)
    {
        var size = Unsafe.SizeOf<T>() * count;
        Offset += size;
        _buffer = _buffer.Slice(size);
    }
    
    public ref T Get<T>()
        where T : unmanaged
    {
        ref var value = ref MemoryMarshal.Cast<byte, T>(_buffer)[0];
        Skip<T>();
        return ref value;
    }
    
    public Span<T> Get<T>(int count)
        where T : unmanaged
    {
        var span = MemoryMarshal.Cast<byte, T>(_buffer).Slice(0, count);
        Skip<T>(count);
        return span;
    }
    
}