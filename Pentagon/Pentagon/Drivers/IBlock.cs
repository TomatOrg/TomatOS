using System;
using System.Threading;
using System.Threading.Tasks;

namespace Pentagon.Drivers;

/// <summary>
/// Represents a block device, which reads/writes in
/// block granularity.
/// </summary>
public interface IBlock
{
    
    /// <summary>
    /// true if the device is removable 
    /// </summary>
    public bool Removable { get; }
    
    /// <summary>
    /// true if the device is currently present in the system
    /// </summary>
    public bool Present { get; }
    
    /// <summary>
    /// true if the block device is in read-only mode.
    /// </summary>
    public bool ReadOnly { get; }
    
    /// <summary>
    /// true if the device caches writes, and flush is needed
    /// to make sure that the blocks are fully written to
    /// the disk.
    /// </summary>
    public bool WriteCaching { get; }
    
    /// <summary>
    /// The size of a single block on the device, read and writes
    /// must be aligned to this size.
    /// </summary>
    public int BlockSize { get; }
    
    /// <summary>
    /// The required alignment for the read/write buffer, otherwise
    /// either a copy ot a failure can happen.
    /// </summary>
    public int IoAlign { get; }
    
    /// <summary>
    /// The last logical block address on the device.
    /// </summary>
    public long LastBlock { get; }

    /// <summary>
    /// The optimal amount of blocks that should be written
    /// in a single request
    /// </summary>
    public int OptimalTransferLengthGranularity { get; }
    
    /// <summary>
    /// Read bytes from lba into the buffer.
    /// The buffer size must be a multiple of the device block size.
    /// </summary>
    public Task ReadBlocks(long lba, Memory<byte> buffer, CancellationToken token = default);
    
    /// <summary>
    /// Write bytes from lba into the buffer.
    /// The buffer size must be a multiple of the device block size.
    /// </summary>
    public Task WriteBlocks(long lba, Memory<byte> buffer, CancellationToken token = default);
    
    /// <summary>
    /// 
    /// </summary>
    /// <param name="token"></param>
    /// <returns></returns>
    public Task FlushBlocks(CancellationToken token = default);

}