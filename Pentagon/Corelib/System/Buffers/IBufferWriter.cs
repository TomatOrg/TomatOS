namespace System.Buffers;

public interface IBufferWriter<T>
{

    public void Advance(int count);

    public Memory<T> GetMemory(int sizeHint = 0);

    public Span<T> GetSpan(int sizeHint = 0);

}