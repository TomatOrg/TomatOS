namespace System.Buffers;

public interface IMemoryOwner<T> : IDisposable
{
    
    public Memory<T> Memory { get; }

}