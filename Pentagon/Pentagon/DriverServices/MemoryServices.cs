using System;
using System.Buffers;
using System.Runtime.CompilerServices;

namespace Pentagon.DriverServices;

public static class MemoryServices
{

    /// <summary>
    /// The base of the kernel's direct map
    /// </summary>
    private const ulong DirectMapBase = 0xffff800000000000ul;
    
    /// <summary>
    /// The size of a page
    /// </summary>
    public static readonly int PageSize = 4096;

    /// <summary>
    /// Get the physical address of memory allocated by AllocatePages, if it was returned
    /// from other methods this may result in InvalidCastException
    /// </summary>
    /// <param name="range">The range of memory to get the physical address for</param>
    /// <returns>The physical address</returns>
    public static ulong GetPhysicalAddress(IMemoryOwner<byte> range)
    {
        // note: we don't need to have this as checked because the object can only be
        //       created by a safe function
        return ((AllocatedMemoryHolder)range)._ptr - 0xffff800000000000ul;
    }

    /// <summary>
    /// Get the physical address of a mapped region, will return -1 if the
    /// address is not actually a direct mapped address.
    /// TODO: do I want to expose this? is there a reason to expose this?
    /// </summary>
    /// <param name="range">The mapped range</param>
    /// <returns>The physical address</returns>
    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    internal static extern ulong GetMappedPhysicalAddress(Memory<byte> range);

    /// <summary>
    /// Allocate pages, which are aligned to the allocation size rounded to the next power of two,
    /// so it is at least page aligned
    /// </summary>
    public static IMemoryOwner<byte> AllocatePages(int pages)
    {
        // make sure we have a good amount of pages
        if (pages < 0)
            throw new ArgumentOutOfRangeException(nameof(pages));

        // create the owner and allocate it 
        var holder = new AllocatedMemoryHolder();
        holder._ptr = AllocateMemory((ulong)pages * (ulong)PageSize);
        if (holder._ptr == 0)
            throw new OutOfMemoryException();

        // update the memory reference in the holder
        UpdateMemory(ref holder._memory, holder, holder._ptr, pages * PageSize);
        
        // return the holder
        return holder;
    }
    
    /// <summary>
    /// Map a range of memory, this can be unaligned both in pointer and size. It is completely
    /// safe to call this multiple times on the same or overlapping ranges
    /// </summary>
    /// <param name="ptr">The physical address</param>
    /// <param name="size">The amount of memory to map</param>
    /// <returns>The Memory object representing the mapped memory</returns>
    internal static Memory<byte> Map(ulong ptr, int size)
    {
        if (size < 0)
            throw new ArgumentOutOfRangeException(nameof(size));
        
        // TODO: chcked
        var rangeStart = KernelUtils.AlignDown(ptr, (ulong)PageSize);
        var rangeEnd = KernelUtils.AlignUp(ptr + (ulong)size, (ulong)PageSize);

        var offset = ptr - rangeStart;
        var pageCount = (rangeEnd - rangeStart) / (ulong)PageSize;
        
        // map, we are going to map the whole page range but only give a reference
        // to the range that we want from it 
        var memory = Memory<byte>.Empty;
        var mapped = MapMemory(rangeStart, pageCount);
        UpdateMemory(ref memory, null, mapped + offset, size);
        return memory;
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

    #region Native Functions

    /// <summary>
    /// Allows us to update the memory structure directly from the kernel, this is not normally possible
    /// because the Corelib abstractions hide the raw access to the pointer
    /// </summary>
    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    internal static extern void UpdateMemory(ref Memory<byte> memory, object holder, ulong ptr, int size);

    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    internal static extern ulong GetSpanPtr(ref Span<byte> memory);
    
    [MethodImpl(MethodCodeType = MethodCodeType.Runtime)]
    internal static extern ref T UnsafePtrToRef<T>(ulong ptr);
    
    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    private static extern ulong AllocateMemory(ulong size);
    
    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    private static extern void FreeMemory(ulong ptr);
    
    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    private static extern ulong MapMemory(ulong ptr, ulong pages);
    
    #endregion

}