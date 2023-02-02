using System;
using System.Buffers;
using System.Numerics;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Tomato.Hal;

public class DmaBuffer : IMemoryOwner<byte>
{

    /// <summary>
    /// The base of the kernel's direct map
    /// </summary>
    private const ulong DirectMapBase = 0xffff800000000000ul;

    /// <summary>
    /// The min allocation size for physical memory
    /// </summary>
    public static readonly int MinSize = MemoryServices.PageSize;

    /// <summary>
    /// The max allocation size for physical memory
    /// </summary>
    public static readonly int MaxSize = MemoryServices.PageSize * 512;
    
    /// <summary>
    /// The virtual buffer
    /// </summary>

    public Memory<byte> Memory { get; }
    
    /// <summary>
    /// The physical address of the buffer, can be used by hardware 
    /// </summary>
    public ulong PhysicalAddress { get; }

    /// <summary>
    /// Allocates a new capable DmaBuffer
    /// </summary>
    /// <param name="size"></param>
    /// <exception cref="ArgumentException"></exception>
    public DmaBuffer(int size)
    {
        // check the size is valid
        size = (int)MemoryServices.AlignUp((ulong)size, (ulong)MemoryServices.PageSize);

        // allocate the memory
        var ptr = MemoryServices.AllocateMemory((ulong)size);
        if (ptr == 0)
            throw new OutOfMemoryException();

        PhysicalAddress = ptr - DirectMapBase;
        
        // fill the memory object
        var memory = new Memory<byte>();
        MemoryServices.UpdateMemory(ref memory, this, ptr, size);
        Memory = memory;
    }

    ~DmaBuffer()
    {
        // remove the reference from this class
        // TODO: this is not good since right now this can cause a use-after-free
        //       if only the span is left and the object is not 
        MemoryServices.FreeMemory(PhysicalAddress + DirectMapBase);
    }

    public Memory<T> AsMemory<T>() where T : unmanaged
    {
        return MemoryMarshal.Cast<byte, T>(Memory);
    }

    public Span<T> AsSpan<T>() where T : unmanaged
    {
        return AsMemory<T>().Span;
    }

    public void Dispose()
    {
        // nothing to do for now, memory management is lazy
    }
    

}