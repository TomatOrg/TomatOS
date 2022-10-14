using Tomato.Resources;
using System;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using Tomato.DriverServices;
using Tomato.DriverServices.Pci;

namespace Tomato.Drivers.Virtio;

public class VirtioBlock : VirtioPciDevice
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

    public static VirtioBlock block;

    internal static bool CheckDevice(PciDevice device)
    {
        if (device.DeviceId != 0x1001 && device.DeviceId != 0x1042)
            return false;

        block = new VirtioBlock(device);
        
        return true;
    }


    public VirtioBlock(PciDevice a) : base(a)
    {
        _devConfig = new VirtioBlkConfig(_devCfgRegion);
        _lastBlock = (long)_devConfig.Capacity.Value - 1;
        var iw = new Thread(IrqWaiterThread);
        iw.Start();
    }
    
    void Process()
    {
        // 1.2 spec, 2.7.14 Receiving Used Buffers From The Device
        while (_queueInfo.LastSeenUsed != _queueInfo.Used.DescIdx.Value)
        {
            // get
            var head = _queueInfo.Used.Ring.Span[_queueInfo.LastSeenUsed % _queueInfo.Size].Id;

            // process
            var t = new Thread(_queueInfo.Completions[head].SetResult);
            t.Start();

            // free
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
        var tcs = new TaskCompletionSource();

        // start io
        var r = MemoryServices.AllocatePages(1);
        var rPhys = MemoryServices.GetPhysicalAddress(r);
        var statusPhys = rPhys + 16;

        var rr = new Region(r.Memory).CreateMemory<BlkReq>(0, 1);
        rr.Span[0].Type = write ? 1u : 0u;
        rr.Span[0].Sector = sector;

        lock (_queueInfo)
        {
            var head = _queueInfo.GetNewDescriptor();
            _queueInfo.Completions[head] = tcs;

            var h = head;
            _queueInfo.Descriptors.Span[h].Phys = rPhys;
            _queueInfo.Descriptors.Span[h].Len = 16;
            _queueInfo.Descriptors.Span[h].Flags = QueueInfo.Descriptor.Flag.HasNext;

            h = _queueInfo.GetNext(h);
            _queueInfo.Descriptors.Span[h].Phys = phys;
            _queueInfo.Descriptors.Span[h].Len = bytes;
            _queueInfo.Descriptors.Span[h].Flags = QueueInfo.Descriptor.Flag.HasNext | (write ? 0 : QueueInfo.Descriptor.Flag.Write);

            h = _queueInfo.GetNext(h);
            _queueInfo.Descriptors.Span[h].Phys = statusPhys;
            _queueInfo.Descriptors.Span[h].Len = 1;
            _queueInfo.Descriptors.Span[h].Flags = QueueInfo.Descriptor.Flag.Write;

            _queueInfo.PlaceHeadOnAvail(head);
            _queueInfo.Notify();
        }

        return tcs.Task;
    }


    [StructLayout(LayoutKind.Sequential)]
    internal struct BlkReq
    {
        public uint Type;
        internal uint _0;
        public ulong Sector;
    }
}