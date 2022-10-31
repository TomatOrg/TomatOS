using System;
using System.Runtime.InteropServices;
using System.Buffers;
using System.Diagnostics;
using System.Collections.Generic;
using System.Threading.Tasks;
using Tomato.Hal.Interfaces;
using Tomato.Hal.Managers;
using Tomato.Hal.Pci;
using Tomato.Hal;

namespace Tomato.Drivers.Virtio;

// Modern adn transitional PCI vendor-device pairs
// Look at 4.1 Virtio over PCI Bus and 5. Device types in the 1.2 spec
[PciDriver(0x1AF4, 0x1042)]
[PciDriver(0x1AF4, 0x1001)]
public class VirtioBlock : VirtioPci
{
    public long _lastBlock;

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
        Test();
    }

    void Test()
    {
        // where to store the data
        var data = MemoryServices.AllocatePhysicalMemory(512);

        // allocate the block IO metadata
        // there are 16 bytes of header, and 1 byte of footer (statuscode)
        var desc = MemoryServices.AllocatePhysicalMemory(16+1);
        var descPhys = MemoryServices.GetPhysicalAddress(desc);
        var statusPhys = descPhys + 16;

        var rr = new Region(desc.Memory).CreateMemory<BlkReq>(0, 1);
        rr.Span[0].Type = 0u;
        rr.Span[0].Sector = 0;

        var head = _queueInfo.GetNewDescriptor();
        
        var h = head;
        _queueInfo.Descriptors.Span[h].Phys = descPhys;
        _queueInfo.Descriptors.Span[h].Len = 16;
        _queueInfo.Descriptors.Span[h].Flags = QueueInfo.Descriptor.Flag.HasNext;

        h = _queueInfo.GetNext(h);
        _queueInfo.Descriptors.Span[h].Phys = MemoryServices.GetPhysicalAddress(data);
        _queueInfo.Descriptors.Span[h].Len = 512;
        _queueInfo.Descriptors.Span[h].Flags = QueueInfo.Descriptor.Flag.HasNext | QueueInfo.Descriptor.Flag.Write;

        h = _queueInfo.GetNext(h);
        _queueInfo.Descriptors.Span[h].Phys = statusPhys;
        _queueInfo.Descriptors.Span[h].Len = 1;
        _queueInfo.Descriptors.Span[h].Flags = QueueInfo.Descriptor.Flag.Write;
        _queueInfo.PlaceHeadOnAvail(head);
        _queueInfo.Notify();

        // syncronously wait for completion
        _queueInfo.Interrupt.Wait();

        for (int i = 0; i < 512; i += 16)
        {
            Debug.Print($"{Read(i+0):x04} {Read(i+2):x04} {Read(i+4):x04} {Read(i+6):x04} {Read(i+8):x04} {Read(i+10):x04} {Read(i+12):x04} {Read(i+14):x04}");
        }

        ushort Read(int i) => (ushort)(((data.Memory.Span[i]) << 8) | data.Memory.Span[i+1]);
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct BlkReq
    {
        public uint Type;
        internal uint _0;
        public ulong Sector;
    }
}