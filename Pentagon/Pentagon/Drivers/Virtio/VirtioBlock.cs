using Pentagon.DriverServices;
using Pentagon.DriverServices.Pci;
using System.Runtime.InteropServices;

namespace Pentagon.Drivers.Virtio;

public class VirtioBlock : VirtioPciDevice
{
    internal static bool CheckDevice(PciDevice device)
    {
        if (device.DeviceId != 0x1001 && device.DeviceId != 0x1042)
            return false;

        block = new(device);

        return true;
    }

    static VirtioBlock block;

    public VirtioBlock(PciDevice a) : base(a)
    {
        Read(69);
    }

    void Read(ulong sector)
    {
        // start io
        var r = MemoryServices.AllocatePages(1);
        var rPhys = MemoryServices.GetPhysicalAddress(r);
        var diskPhys = rPhys + 512;
        var statusPhys = rPhys + 1024;

        var rr = new Region(r.Memory).CreateMemory<BlkReq>(0, 1);
        rr.Span[0].Type = 0;
        rr.Span[0].Sector = sector;

        var h = _queueInfo.GetNewDescriptor(true);
        _queueInfo.Descriptors.Span[h].Phys = rPhys;
        _queueInfo.Descriptors.Span[h].Len = 16;
        _queueInfo.Descriptors.Span[h].Flags = QueueInfo.Descriptor.Flag.HasNext;

        h = _queueInfo.GetNext(h);
        _queueInfo.Descriptors.Span[h].Phys = diskPhys;
        _queueInfo.Descriptors.Span[h].Len = 512;
        _queueInfo.Descriptors.Span[h].Flags = QueueInfo.Descriptor.Flag.HasNext | QueueInfo.Descriptor.Flag.Write;

        h = _queueInfo.GetNext(h);
        _queueInfo.Descriptors.Span[h].Phys = statusPhys;
        _queueInfo.Descriptors.Span[h].Len = 1;
        _queueInfo.Descriptors.Span[h].Flags = QueueInfo.Descriptor.Flag.Write;

        _queueInfo.Notify();

        _queueInfo.Interrupt.Wait();

        // 1.2 spec, 2.7.14 Receiving Used Buffers From The Device
        while (_queueInfo.LastSeenUsed != _queueInfo.Used.DescIdx.Value)
        {
            // get
            var head = _queueInfo.Used.Ring.Span[_queueInfo.LastSeenUsed % _queueInfo.Size].Id;

            // process
            Log.LogHex(head);

            // free
            // NOTE: this also increases LastSeenUsed
            _queueInfo.FreeChain(head);
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct BlkReq
    {
        public uint Type;
        internal uint _0;
        public ulong Sector;
    }
}