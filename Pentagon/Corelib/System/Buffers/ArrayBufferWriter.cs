namespace System.Buffers;

public sealed class ArrayBufferWriter<T> : IBufferWriter<T>
{

    private T[] _buffer;
    private int _writtenCount;

    public int Capacity => _buffer.Length;
    public int FreeCapacity => Capacity - _writtenCount;
    public int WrittenCount => _writtenCount;
    
    // TODO: written memory
    // TODO: written span
    
    public ArrayBufferWriter()
    {
        _buffer = Array.Empty<T>();
        _writtenCount = 0;
    }

    public ArrayBufferWriter(int initialCapacity)
    {
        _buffer = initialCapacity == 0 ? Array.Empty<T>() : new T[initialCapacity];
        _writtenCount = 0;
    }

    public void Advance(int count)
    {
        if (count < 0)
            throw new ArgumentException(null, nameof(count));

        if (_writtenCount > _buffer.Length - count)
            throw new InvalidOperationException("Cannot advance past the end of the buffer");

        _writtenCount += count;
    }

    public void Clear()
    {
        _buffer.AsSpan(0, _writtenCount).Clear();
        _writtenCount = 0;
    }

    private void CheckAndResizeBuffer(int sizeHint)
    {
        if (sizeHint < 0)
            throw new ArgumentException(nameof(sizeHint));

        if (sizeHint == 0)
        {
            sizeHint = 1;
        }

        if (sizeHint > FreeCapacity)
        {
            var growBy = Math.Max(sizeHint, Capacity);
            if (Capacity == 0)
            {
                growBy = Math.Max(growBy, 256);
            }

            var newSize = Capacity + growBy;
            if ((uint)newSize > int.MaxValue)
            {
                var needed = (uint)(Capacity - FreeCapacity + sizeHint);
                if (needed > Array.MaxLength)
                {
                    throw new OutOfMemoryException("Cannot allocate a buffer.");
                }

                newSize = Array.MaxLength;
            }
            
            Array.Resize(ref _buffer, newSize);
        }
    }
    
    public Memory<T> GetMemory(int sizeHint = 0)
    {
        CheckAndResizeBuffer(sizeHint);
        return _buffer.AsMemory(_writtenCount);
    }

    public Span<T> GetSpan(int sizeHint = 0)
    {
        CheckAndResizeBuffer(sizeHint);
        return _buffer.AsSpan(_writtenCount);
    }
}