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

/// <summary>
/// A virtio *device*, this class is inherited by the specific device classes
/// And it's never meant to be used directly
/// </summary>
public abstract class VirtioPci
{
    /// <summary>
    /// State relative to a single virtqueue
    /// </summary>
    /// <remarks>On some virtio devices, each queue has a separate function and they aren't interchangeable</remarks>
    public class QueueInfo
    {
        // Size of the queue in elements
        readonly public int Size;
        // Index of the queue inside the device, only used for notification
        readonly int Index;

        // Allocated memory and partitions
        readonly DmaBuffer _backingMemory;
        readonly public Memory<Descriptor> Descriptors;
        readonly public AvailRing Avail;
        readonly public UsedRing Used;

        ushort FirstFree;
        ushort LastFree;
        public volatile ushort LastSeenUsed;

        // Optimization endorsed by the spec: instead of updating Avail.DescIdx each time, batch and do a single notification at the end
        // this variable keeps track of how much to increment DescIdx when notifying
        ushort AddedHeads;

        internal ulong DescPhys, AvailPhys, UsedPhys;
        Field<ushort> Notifier;
        readonly internal Irq Interrupt;

        internal QueueInfo(int index, int size, Field<ushort> notifier, Irq interrupt)
        {
            Index = index;
            Size = size;
            Notifier = notifier;
            Interrupt = interrupt;

            // calculate the size to allocate
            // TODO: those can be separate allocations, but this code should be replaced with the slightly faster packed virtqueue format
            var descrSize = 16 * Size;
            var availSize = 6 + 2 * Size;
            var usedSize = 6 * 8 * Size;

            var total = descrSize + availSize + usedSize;
            _backingMemory = new DmaBuffer(total);
            var r = new Region(_backingMemory.Memory);

            // set the three communication regions
            Descriptors = r.CreateMemory<Descriptor>(0, Size);
            Avail = new(r.CreateRegion(descrSize, availSize), Size);
            Used = new(r.CreateRegion(descrSize + availSize, usedSize), Size);

            DescPhys = _backingMemory.PhysicalAddress;
            AvailPhys = DescPhys + (ulong)descrSize;
            UsedPhys = AvailPhys + (ulong)availSize;

            // initialize descriptor linked list
            for (int i = 0; i < Size - 1; i++) Descriptors.Span[i].NextDescIdx = (ushort)(i + 1);
            Descriptors.Span[Size - 1].NextDescIdx = 0xFFFF; // last entry, point it to an invalid value

            // initialize bookkeeping fields
            FirstFree = 0;
            LastFree = (ushort)(Size - 1);
            LastSeenUsed = 0;
            AddedHeads = 0;
        }

        /// <summary>
        /// Allocate a descriptor
        /// </summary>
        public ushort GetNewDescriptor()
        {
            var oldFirstFree = FirstFree;
            FirstFree = Descriptors.Span[oldFirstFree].NextDescIdx;
            return oldFirstFree;
        }

        /// <summary>
        /// Get a new descriptor and link it as the next one in the current chain
        /// </summary>
        public ushort GetNext(ushort curr)
        {
            var next = GetNewDescriptor();
            Descriptors.Span[curr].NextDescIdx = next;
            Descriptors.Span[curr].Flags |= Descriptor.Flag.HasNext;
            return next;
        }

        /// <summary>
        /// Place descriptor chain head on the avail ring, but don't notify
        /// </summary>
        public void PlaceHeadOnAvail(ushort head)
        {
            Avail.Ring.Span[(Avail.DescIdx.Value + AddedHeads) % Size] = head;
            AddedHeads++;
        }

        /// <summary>
        /// Commit all the descriptor heads pushed with GetNewDescriptor(true) and notify the device.
        /// </summary>
        public void Notify()
        {
            Avail.DescIdx.Value += AddedHeads;
            AddedHeads = 0;
            Notifier.Value = (ushort)Index;
        }

        /// <summary>
        /// Free the descriptor chain starting with `head`
        /// </summary>
        public void FreeChain(ushort head)
        {
            Descriptors.Span[LastFree].Flags = Descriptor.Flag.HasNext;
            Descriptors.Span[LastFree].NextDescIdx = head;
            ushort tail = head;
            while ((Descriptors.Span[tail].Flags & Descriptor.Flag.HasNext) != 0) tail = Descriptors.Span[tail].NextDescIdx;
            LastFree = tail;
        }

        /// <summary>
        /// The packed structure of a descriptor inside a queue
        /// <para>A descriptor is the structure representing a memory region passed to virtio,
        /// with an intrusive linked list (NextDescIdx) to link it to the next one for the current transfer.</para>
        /// </summary>
        /// <see>AvailRing</see>
        [StructLayout(LayoutKind.Sequential)]
        public struct Descriptor
        {
            internal ulong Phys;
            internal uint Len;
            internal Flag Flags;
            internal ushort NextDescIdx; // Next field as an index inside Descriptors

            internal enum Flag : ushort
            {
                HasNext = 1,
                Write = 2,
            }
        }

        /// <summary>
        /// Also known as the driver section
        /// because the driver puts the descriptor chain heads here to start IO
        /// <para>After the linked list of <c>Descriptor</c> has been set up,
        /// the head of the chain is put in this ringbuffer</para>
        /// </summary>
        public class AvailRing
        {
            internal Field<ushort> Flags;
            internal Field<ushort> DescIdx; // Index of what would be the *next* descriptor entry
            internal Memory<ushort> Ring;

            internal AvailRing(Region r, int size)
            {
                Flags = r.CreateField<ushort>(0);
                DescIdx = r.CreateField<ushort>(2);
                Ring = r.CreateMemory<ushort>(4, size);
            }
        }

        /// <summary>
        /// Also known as the device section
        /// because the device puts descriptor chains here once the IO has completed and they can be reused
        /// </summary>
        public class UsedRing
        {
            internal Field<ushort> Flags;
            internal Field<ushort> DescIdx;
            internal Memory<UsedElement> Ring;

            internal UsedRing(Region r, int size)
            {
                Flags = r.CreateField<ushort>(0);
                DescIdx = r.CreateField<ushort>(2);
                Ring = r.CreateMemory<UsedElement>(4, size);
            }

            [StructLayout(LayoutKind.Sequential)]
            internal struct UsedElement
            {
                internal ushort Id; // Index (in Descriptors) of the head of the chain
                private readonly ushort _; // Padding, some documentation notes Id as a LE 32bit field, but it's the same
                internal uint Len; // Number of bytes written into the buffers in the descriptor chain
            }

        }
    }

    public class VirtioPciCommonCfg
    {
        // Whole device: can be set regardless of the current queue
        public Field<uint> DeviceFeatureSelect;
        public Field<uint> DeviceFeature;
        public Field<uint> DriverFeatureSelect;
        public Field<uint> DriverFeature;
        public Field<ushort> MsixConfig;
        public Field<ushort> NumQueues;
        public Field<DevStatus> DeviceStatus;
        public Field<byte> ConfigGeneration;

        // Queue-specific: put a queue number in QueueSelect to get that queue's regs
        public Field<ushort> QueueSelect;
        public Field<ushort> QueueSize;
        public Field<ushort> QueueMsixVector;
        public Field<ushort> QueueEnable;
        public Field<ushort> QueueNotifyOff;
        public Field<ulong> QueueDesc;
        public Field<ulong> QueueDriver;
        public Field<ulong> QueueDevice;

        public VirtioPciCommonCfg(Region r)
        {
            DeviceFeatureSelect = r.CreateField<uint>(0);
            DeviceFeature = r.CreateField<uint>(4);
            DriverFeatureSelect = r.CreateField<uint>(8);
            DriverFeature = r.CreateField<uint>(12);
            MsixConfig = r.CreateField<ushort>(16);
            NumQueues = r.CreateField<ushort>(18);
            DeviceStatus = r.CreateField<DevStatus>(20);
            ConfigGeneration = r.CreateField<byte>(21);

            QueueSelect = r.CreateField<ushort>(22);
            QueueSize = r.CreateField<ushort>(24);
            QueueMsixVector = r.CreateField<ushort>(26);
            QueueEnable = r.CreateField<ushort>(28);
            QueueNotifyOff = r.CreateField<ushort>(30);
            QueueDesc = r.CreateField<ulong>(32);
            QueueDriver = r.CreateField<ulong>(40);
            QueueDevice = r.CreateField<ulong>(48);
        }

        public enum DevStatus : byte
        {
            Acknowledge = 1,
            Driver = 2,
            FeaturesOk = 8,
            DriverOk = 4,
        }
    }

    protected PciDevice _pci;
    protected Region _devCfgRegion;
    protected VirtioPciCommonCfg _common;
    protected QueueInfo _queueInfo;
    protected Region _notify;
    readonly private uint _notifyMultiplier;

    [StructLayout(LayoutKind.Sequential)]
    private struct Capability
    {
        PciCapability Header;
        public byte _0; // cap_len in the virtio spec
        public CfgType Type;
        public byte Bar;
        private byte _1, _2, _3;
        public uint Offset;
        public uint Length;
        public uint NotifyOffMultiplier;

        public enum CfgType : byte
        {
            CommonCfg = 1,
            NotifyCfg = 2,
            IsrCfg = 3,
            DeviceCfg = 4,
            PciCfg = 5,
            SharedMemoryCfg = 8,
            VendorCfg = 9,
        };
    }

    public VirtioPci(PciDevice pci)
    {
        _pci = pci;
        foreach (var cap in _pci.Capabilities)
        {
            if (cap.Span[0] == 0x09)
            {
                var mem = MemoryMarshal.Cast<byte, Capability>(cap);
                ref var virtioCap = ref mem.Span[0];
                var bar = _pci.MemoryBars[virtioCap.Bar];

                // ignore IO bars
                // there are legitimate usecases for them, but they're not useful *yet*
                // (according to the virtio spec, they can be used for faster notifications on VMs)

                var off = virtioCap.Offset;
                var len = virtioCap.Length;
                var type = virtioCap.Type;

                var slice = new Region(bar.Slice((int)off, (int)len));

                if (type == Capability.CfgType.CommonCfg)
                {
                    _common = new VirtioPciCommonCfg(slice);
                }
                else if (type == Capability.CfgType.NotifyCfg)
                {
                    _notify = slice;
                    _notifyMultiplier = virtioCap.NotifyOffMultiplier;
                } else if (type == Capability.CfgType.DeviceCfg)
                {
                    _devCfgRegion = slice;
                }
            }
        }

        _common.DeviceStatus.Value = 0; // reset
        _common.DeviceStatus.Value |= VirtioPciCommonCfg.DevStatus.Acknowledge; // it exists
        _common.DeviceStatus.Value |= VirtioPciCommonCfg.DevStatus.Driver; // it can be loaded

        ulong requiredFeatures = (1ul << 32); // VIRTIO_F_VERSION_1
        ulong optionalFeatures = 0;
        for (int i = 0; i < 2; i++)
        {
            _common.DeviceFeatureSelect.Value = (uint)i;
            _common.DeviceFeature.Value = (uint)((requiredFeatures >> (i * 32)) | (optionalFeatures >> (i * 32)));
            _common.DriverFeatureSelect.Value = (uint)i;
            // TODO: check that the two are compatible
        }

        // features acknowledged
        _common.DeviceStatus.Value |= VirtioPciCommonCfg.DevStatus.FeaturesOk;
        
        // TODO: enable all queues and not just the first
        _pci.Msix.Configure(1);
        for (int q = 0; q < 1; q++)
        {
            _common.QueueSelect.Value = (ushort)q;
            int size = _common.QueueSize.Value;
            // if the size is zero, the queue has to be ignored
            if (size == 0) continue;
            // TODO: support packed virtqueues instead
            var msixIdx = q;

            _queueInfo = new(q, size, _notify.CreateField<ushort>(q * (int)_notifyMultiplier), _pci.Msix[msixIdx]);
            _common.QueueDesc.Value = _queueInfo.DescPhys;
            _common.QueueDriver.Value = _queueInfo.AvailPhys;
            _common.QueueDevice.Value = _queueInfo.UsedPhys;
            _common.QueueMsixVector.Value = (ushort)msixIdx;

            // and finally, enable the queue
            _common.QueueEnable.Value = 1;
        }

        // ready to work
        _common.DeviceStatus.Value |= VirtioPciCommonCfg.DevStatus.DriverOk;
    }
}
