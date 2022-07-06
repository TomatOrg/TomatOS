using System;
using System.Buffers;
using System.Runtime.CompilerServices;

namespace Pentagon;

public static class MemoryServices
{

    /// <summary>
    /// Allocate pages, which are aligned to the allocation size rounded to the next power of two,
    /// so it is at least page aligned
    /// </summary>
    public static IMemoryOwner<byte> AllocatePages(int pages)
    {
        if (pages < 0)
            throw new ArgumentOutOfRangeException(nameof(pages));

        var holder = new AllocatedMemoryHolder();
        holder._ptr = AllocateMemory((ulong)pages * 4096);
        if (holder._ptr == 0)
            throw new OutOfMemoryException();
        UpdateMemory(ref holder._memory, holder, holder._ptr, pages * 4096);
        return holder;
    }
    
    /// <summary>
    /// Map physical pages, returning a Memory range which is usable
    /// for other stuff
    /// </summary>
    internal static IMemoryOwner<byte> MapPages(ulong ptr, int pages)
    {
        var holder = new MappedMemoryHolder();
        var mapped = MapMemory(ptr, (ulong)pages * 4096);
        UpdateMemory(ref holder._memory, holder, mapped, pages * 4096);
        return holder;
    }

    /// <summary>
    /// Holds a reference to memory which is allocated
    /// </summary>
    private sealed class AllocatedMemoryHolder : IMemoryOwner<byte>
    {

        internal Memory<byte> _memory = Memory<byte>.Empty;
        internal ulong _ptr = 0;

        public Memory<byte> Memory => _memory;

        ~AllocatedMemoryHolder()
        {
            Dispose();
        }
        
        public void Dispose()
        {
            if (_memory.IsEmpty)
                return;

            FreeMemory(_ptr);
            GC.SuppressFinalize(this);
            
            _memory = Memory<byte>.Empty;
        }
        
    }
    
    /// <summary>
    /// Holds a reference to memory which is memory mapped
    /// </summary>
    private sealed class MappedMemoryHolder : IMemoryOwner<byte>
    {

        internal Memory<byte> _memory = Memory<byte>.Empty;

        public Memory<byte> Memory => _memory;
        
        public void Dispose()
        {
        }
        
    }

    #region Native

    /// <summary>
    /// Allows us to update the memory structure directly from the kernel, this is not normally possible
    /// because the Corelib abstractions hide the raw access to the pointer
    /// </summary>
    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern void UpdateMemory(ref Memory<byte> memory, object holder, ulong ptr, int size);
    
    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern ulong AllocateMemory(ulong size);
    
    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern void FreeMemory(ulong ptr);
    
    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern ulong MapMemory(ulong ptr, ulong size);
    
    #endregion

}