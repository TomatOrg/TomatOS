using System;
using System.Runtime.InteropServices;
using System.Buffers;
using System.Threading;
using System.Diagnostics;
using System.Collections.Generic;
using System.Threading.Tasks;
using System.Runtime.CompilerServices;
using Tomato.Hal.Interfaces;
using Tomato.Hal.Managers;
using Tomato.Hal.Pci;
using Tomato.Hal.Io;
using Tomato.Hal;
using static Tomato.Hal.MemoryServices;

namespace Tomato.Drivers.Virtio;

// Modern adn transitional PCI vendor-device pairs
// Look at 4.1 Virtio over PCI Bus and 5. Device types in the 1.2 spec
[PciDriver(0x1AF4, 0x1042)]
[PciDriver(0x1AF4, 0x1001)]
public class VirtioBlock : VirtioPci, IBlock
{
    // TODO: use VIRTIO_BLK_F_FLUSH and VIRTIO_BLK_F_TOPOLOGY

    public bool Removable => false;
    public bool Present => true;
    public bool ReadOnly => false;
    public bool WriteCaching => false;    
    public long LastBlock => _lastBlock;
    public int BlockSize => 512;
    public int IoAlign => 512;
    public int OptimalTransferLengthGranularity => 8;

    public Task ReadBlocks(long lba, Memory<byte> memory, CancellationToken token = default)
        => DoAsync((ulong)lba, (uint)memory.Length, false, GetMappedPhysicalAddress(memory));

    public Task WriteBlocks(long lba, Memory<byte> memory, CancellationToken token = default)
        => DoAsync((ulong)lba, (uint)memory.Length, true, GetMappedPhysicalAddress(memory));

    public Task FlushBlocks(CancellationToken token = default)
        => Task.CompletedTask;

    private long _lastBlock;
    VirtioBlkConfig _devConfig;
    public class VirtioBlkConfig
    {
        public Field<ulong> Capacity;

        public VirtioBlkConfig(Region r)
        {
            Capacity = r.CreateField<ulong>(0);
        }
    }

    public VirtioBlock(PciDevice a) : base(a)
    {
        _devConfig = new VirtioBlkConfig(_devCfgRegion);
        _lastBlock = (long)_devConfig.Capacity.Value - 1;
        _blockPackets = new PacketInfo[_queueInfo.Size];
        _blockAlloc = new();
        (new Thread(IrqWaiterThread)).Start();
        Debug.Print("VirtioBlock: Device registered");
        BlockManager.RegisterBlock(this);
    }

    void Process()
    {
        // 1.2 spec, 2.7.14 Receiving Used Buffers From The Device
        while (_queueInfo.LastSeenUsed != _queueInfo.Used.DescIdx.Value)
        {
            // get
            var head = _queueInfo.Used.Ring.Span[_queueInfo.LastSeenUsed % _queueInfo.Size].Id;
            ref var pkt = ref _blockPackets[head];
            ref var blkPkt = ref _blockAlloc[pkt.PacketIdx].Span[0];

            // process
            pkt.Tcs.SetResult();

            // free buffers used
            _blockAlloc.Free(pkt.PacketIdx);

            // free virtio-related bookkeeping
            // NOTE: this also increases LastSeenUsed
            _queueInfo.FreeChain(head);
        }
    }

    void IrqWaiterThread()
    {
        while (true)
        {
            _queueInfo.Interrupt.Wait();
            Process();
        }
    }
    Task DoAsync(ulong sector, uint bytes, bool write, ulong phys)
    {
        var tcs = new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously);

        // allocate the block IO metadata
        _blockAlloc.Allocate(out ulong address, out Memory<BlockPacket> pkt, out uint index);
        pkt.Span[0].InReq.Type = write ? 1u : 0u;
        pkt.Span[0].InReq.Sector = sector;

        lock (_queueInfo)
        {
            var head = _queueInfo.GetNewDescriptor();
            _blockPackets[head] = new PacketInfo(index, tcs);
            var h = head;
            _queueInfo.Descriptors.Span[h].Phys = address;
            _queueInfo.Descriptors.Span[h].Len = 16;
            _queueInfo.Descriptors.Span[h].Flags = QueueInfo.Descriptor.Flag.HasNext;

            h = _queueInfo.GetNext(h);
            _queueInfo.Descriptors.Span[h].Phys = phys;
            _queueInfo.Descriptors.Span[h].Len = bytes;
            _queueInfo.Descriptors.Span[h].Flags = QueueInfo.Descriptor.Flag.HasNext | (write ? 0 : QueueInfo.Descriptor.Flag.Write);

            h = _queueInfo.GetNext(h);
            _queueInfo.Descriptors.Span[h].Phys = address + 16;
            _queueInfo.Descriptors.Span[h].Len = 1;
            _queueInfo.Descriptors.Span[h].Flags = QueueInfo.Descriptor.Flag.Write;
            _queueInfo.PlaceHeadOnAvail(head);
            _queueInfo.Notify();
        }

        return tcs.Task;
    }

    PacketAllocator<BlockPacket> _blockAlloc;

    [StructLayout(LayoutKind.Sequential)]
    internal struct BlockPacket
    {
        internal BlkReq InReq;
        internal byte Status;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct BlkReq
    {
        internal uint Type;
        internal uint _0;
        internal ulong Sector;
    }

    PacketInfo[] _blockPackets;

    internal struct PacketInfo
    {
        internal uint PacketIdx;
        internal TaskCompletionSource Tcs;
        
        public PacketInfo(uint idx, TaskCompletionSource tcs)
        {
            PacketIdx = idx;
            Tcs = tcs;
        }
    }

    private class PacketAllocator<T> where T : unmanaged
    {
        List<IMemoryOwner<byte>> _pages = new();
        uint _firstFree = 0xFFFFFFFF; // high 16 bits = page, low 16 bits = idx
        int _size = Unsafe.SizeOf<T>();

        private void Fill()
        {
            var pageIdx = _pages.Count;
            var mem = AllocatePhysicalMemory(PageSize);
            var s = MemoryMarshal.Cast<byte, uint>(mem.Memory).Span;
            var max = (PageSize - 4) / (_size * 4);
            for (int i = 0; i < max; i++)
            {
                s[i * _size] = ((uint)pageIdx << 8) | (uint)((i * 1) * _size);
            }
            s[0] = 0; // TODO: put number of used entries in page so i can reclaim the page to the allocator
            s[max] = 0xFFFFFFFF;
            
            _pages.Add(mem);
            _firstFree = (uint)(pageIdx) << 16;
        }

        internal void Allocate(out ulong address, out Memory<T> mem, out uint index)
        {
            uint curr = _firstFree;
            if (_firstFree == 0xFFFFFFFF) { Fill(); curr = _firstFree; }
            IMemoryOwner<byte> page = _pages[(int)(curr >> 16)];
            int idx = (int)(curr & 0xFFFF);
            uint newFree = (MemoryMarshal.Cast<byte, uint>(page.Memory)).Span[idx];
            _firstFree = newFree;

            address = GetPhysicalAddress(page) + (ulong)idx * 4;
            mem = MemoryMarshal.Cast<byte, T>(page.Memory.Slice(idx * 4));
            index = curr;
        }

        internal void Free(uint index)
        {
            uint pageIdx = index >> 16, idx = index & 0xFFFF;
            MemoryMarshal.Cast<byte, uint>(_pages[(int)pageIdx].Memory).Span[(int)idx] = _firstFree;
            _firstFree = (pageIdx << 16) | idx;
        }

        internal Memory<T> this[uint index]
        {
            get {
                uint pageIdx = index >> 16, idx = index & 0xFFFF;
                return MemoryMarshal.Cast<byte, T>(_pages[(int)pageIdx].Memory.Slice((int)idx * 4));
            }
        }
        
    }
}