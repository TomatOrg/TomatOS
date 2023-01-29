using System;
using System.Threading;
using System.Threading.Tasks;
using Tomato.Hal.Interfaces;

namespace Tomato.Hal.Io;

public class Partition : IBlock
{
    
    public bool Removable => _drive.Removable;
    public bool Present => _drive.Present;
    public bool ReadOnly => _drive.ReadOnly;
    public bool WriteCaching => _drive.WriteCaching;    
    public long LastBlock => _drive.LastBlock;
    public int BlockSize => _drive.BlockSize;
    public int IoAlign => _drive.IoAlign;
    public int OptimalTransferLengthGranularity => _drive.OptimalTransferLengthGranularity;

    private IBlock _drive;
    private long _start, _end;

    public Partition(IBlock drive, long start, long end)
    {
        _drive = drive;
        _start = start;
        _end = end;
    }

    public Task ReadBlocks(long lba, Memory<byte> memory, CancellationToken token = default) => _drive.ReadBlocks(lba + _start, memory, token);

    public Task WriteBlocks(long lba, Memory<byte> memory, CancellationToken token = default) => _drive.WriteBlocks(lba + _start, memory, token);

    public Task FlushBlocks(CancellationToken token = default) => _drive.FlushBlocks(token);
}