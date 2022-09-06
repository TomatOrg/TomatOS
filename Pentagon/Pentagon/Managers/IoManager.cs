using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using Pentagon.Drivers;
using Pentagon.Drivers.Virtio;
using Pentagon.Interfaces;

namespace Pentagon.Managers
{
    class Partition : IBlock
    {
        IBlock _disk;
        public ulong StartLba;
        public ulong EndLba;
        public long _last;
        public Partition(IBlock disk, ulong start, ulong end)
        {
            StartLba = start;
            EndLba = end;
            _last = (long)(end - start) - 1;
            _disk = disk;
        }

        public bool Removable => _disk.Removable;
        public bool Present => _disk.Present;
        public bool ReadOnly => _disk.ReadOnly; // TODO: support readonly mounts
        public bool WriteCaching => _disk.WriteCaching; // TODO: support changing caching mode
        public int BlockSize => _disk.BlockSize;
        public int IoAlign => _disk.IoAlign;
        public int OptimalTransferLengthGranularity => _disk.OptimalTransferLengthGranularity;
        public long LastBlock => _last;

        // TODO: check for out of bound IO
        public Task ReadBlocks(long lba, Memory<byte> buffer, CancellationToken token = default)
        {
            return _disk.ReadBlocks(lba + (long)StartLba, buffer, token);
        }

        public Task WriteBlocks(long lba, Memory<byte> buffer, CancellationToken token = default)
        {
            return _disk.WriteBlocks(lba + (long)StartLba, buffer, token);
        }
        public Task FlushBlocks(CancellationToken token = default) => _disk.FlushBlocks(token);
    }
    class BlockInfo
    {
        public IBlock Device;
        public string Name;
        public List<Partition> PartitionInfo;
        public BlockInfo(IBlock d)
        {
            Device = d;
            Name = null; // TODO:
            PartitionInfo = new();
        }
    }

    class IoManager
    {
        static IBlock pp;
        public static List<BlockInfo> BlockDevices = new();
        public static List<IFileSystem> FSes = new();

        public static async Task AddBlock(IBlock block)
        {
            var bi = new BlockInfo(block);
            await Gpt.IteratePartitions(bi.Device, HandlePartition);
            BlockDevices.Add(bi);

            void HandlePartition(int idx, ulong start, ulong end)
            {
                var p = new Partition(block, start, end);
                bi.PartitionInfo.Add(p);
                pp = p;
                var d = Fat32.CheckDevice(pp);
                d.GetAwaiter().OnCompleted(() => {
                    if (d.Result != null) FSes.Add(d.Result);
                });
            }
        }
    }
}
