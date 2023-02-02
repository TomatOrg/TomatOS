using System;
using System.Buffers;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Tomato.Hal;

public class MemoryServices
{
    
    public static ulong AlignDown(ulong value, ulong alignment)
    {
        return value - (value & (alignment - 1));
    }
    
    public static ulong AlignUp(ulong value, ulong alignment)
    {
        return (value + (alignment - 1)) & ~(alignment - 1);
    }
    
    public static ulong DivideUp(ulong value, ulong alignment)
    {
        return (value + (alignment - 1)) / alignment;
    }
    
    public static int DivideUp(int value, int alignment)
    {
        return (value + (alignment - 1)) / alignment;
    }

    /// <summary>
    /// The size of a page
    /// </summary>
    public static readonly int PageSize = 4096;

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
        var rangeStart = AlignDown(ptr, (ulong)PageSize);
        var rangeEnd = AlignUp(ptr + (ulong)size, (ulong)PageSize);

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
    /// Get the physical address of a mapped region, will return -1 if the
    /// address is not actually a direct mapped address.
    /// </summary>
    /// <param name="range">The mapped range</param>
    /// <returns>The physical address</returns>
    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    public static extern ulong GetMappedPhysicalAddress(Memory<byte> range);

    internal static Memory<T> Map<T>(ulong ptr, int count = 1)
        where T : unmanaged
    {
        return MemoryMarshal.Cast<byte, T>(Map(ptr, Unsafe.SizeOf<T>() * count));
    }
    
    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    internal static extern void UpdateMemory(ref Memory<byte> memory, object holder, ulong ptr, int size);

    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    internal static extern ulong GetSpanPtr(ref Span<byte> memory);
    
    [MethodImpl(MethodCodeType = MethodCodeType.Runtime)]
    internal static extern ref T UnsafePtrToRef<T>(ulong ptr);
    
    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    internal static extern ulong AllocateMemory(ulong size);
    
    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    internal static extern void FreeMemory(ulong ptr);
    
    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    private static extern ulong MapMemory(ulong ptr, ulong pages);

}