using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;
using System.Buffers;
using Pentagon.HAL;

namespace Pentagon
{
    /// <summary>
    /// The *driver* object, it automatically creates supported virtio devices 
    /// </summary>
    public class VirtioPciDriver : IPciDriver
    {
        public bool Init(PciDevice a)
        {
            if (a.Config.VendorId != 0x1AF4) return false;
            // Former is the transitional device ID, latter is the 1.0+ one
            if (a.Config.DeviceId == 0x1001 || a.Config.DeviceId == (0x1040 + 2))
            {
                _ = new VirtioBlockDevice(a);    
                return true;
            }
            return false;
        }
    }

    /// <summary>
    /// A virtio *device*, this class is inherited by the specific device classes
    /// And it's never meant to be used directly
    /// </summary>
    internal class VirtioPciDevice
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
            readonly IMemoryOwner<byte> _backingMemory;
            readonly public Memory<Descriptor> Descriptors;
            readonly public AvailRing Avail;
            readonly public UsedRing Used;

            ushort FirstFree;
            public ushort LastSeenUsed;

            // Optimization endorsed by the spec: instead of updating Avail.DescIdx each time, batch and do a single notification at the end
            // this variable keeps track of how much to increment DescIdx when notifying
            ushort AddedHeads;

            internal ulong DescPhys, AvailPhys, UsedPhys;
            Field<ushort> Notifier;
            readonly public Pci.Msix.Irq Interrupt;

            public QueueInfo(int index, int size, Field<ushort> notifier, Pci.Msix.Irq interrupt)
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
                _backingMemory = MemoryServices.AllocatePages((int)HAL.KernelUtils.AlignUp((ulong)total, (ulong)HAL.MemoryServices.PageSize));
                var r = new Region(_backingMemory.Memory);

                // set the three communication regions
                Descriptors = r.CreateMemory<Descriptor>(0, Size);
                Avail = new(r.CreateRegion(descrSize, availSize), Size);
                Used = new(r.CreateRegion(descrSize + availSize, usedSize), Size);

                DescPhys = MemoryServices.GetPhys(_backingMemory);
                AvailPhys = DescPhys + (ulong)descrSize;
                UsedPhys = AvailPhys + (ulong)availSize;

                // initialize descriptor linked list
                for (int i = 0; i < Size - 1; i++) Descriptors.Span[i].NextDescIdx = (ushort)(i + 1);
                Descriptors.Span[Size - 1].NextDescIdx = 0xFFFF; // last entry, point it to an invalid value

                // initialize bookkeeping fields
                FirstFree = 0;
                LastSeenUsed = 0;
                AddedHeads = 0;
            }

            /// <summary>
            /// Allocate a descriptor, and if it's a head descriptor, place it in the available ring
            /// </summary>
            /// <remarks>Despite being added to the available ring, the IO isn't started until Notify() is called</remarks>
            public ushort GetNewDescriptor(bool isHead)
            {
                var oldFirstFree = FirstFree;
                FirstFree = Descriptors.Span[oldFirstFree].NextDescIdx;
                if (isHead)
                {
                    Avail.Ring.Span[(Avail.DescIdx.Value + AddedHeads) % Size] = oldFirstFree;
                    AddedHeads++;
                }
                return oldFirstFree;
            }

            /// <summary>
            /// Get a new descriptor and link it as the next one in the current chain
            /// </summary>
            public ushort GetNext(ushort curr)
            {
                var next = GetNewDescriptor(false);
                Descriptors.Span[curr].NextDescIdx = next;
                return next;
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
                var desc = head;
                while ((int)(Descriptors.Span[desc].Flags & Descriptor.Flag.HasNext) > 0)
                {
                    desc = Descriptors.Span[desc].NextDescIdx;
                }
                Descriptors.Span[desc].Flags = Descriptor.Flag.HasNext;
                Descriptors.Span[desc].NextDescIdx = FirstFree;
                FirstFree = desc;
                LastSeenUsed++;
            }

            /// <summary>
            /// The packed structure of a descriptor inside a queue
            /// <para>A descriptor is the structure representing a memory region passed to virtio,
            /// with an intrusive linked list (NextDescIdx) to link it to the next one for the current transfer.</para>
            /// </summary>
            /// <see>AvailRing</see>
            [StructLayout(LayoutKind.Sequential)]
            internal struct Descriptor
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
            internal class AvailRing
            {
                internal Field<ushort> Flags;
                internal Field<ushort> DescIdx; // Index of what would be the *next* descriptor entry
                internal Memory<ushort> Ring;

                internal AvailRing(Region r, int size)
                {
                    Flags = r.CreateField<ushort>(0);
                    DescIdx = r.CreateField<ushort>(2);
                    Ring = r.CreateMemory<ushort>(4, size / 2);
                }
            }

            /// <summary>
            /// Also known as the device section
            /// because the device puts descriptor chains here once the IO has completed and they can be reused
            /// </summary>
            internal class UsedRing
            {
                internal Field<ushort> Flags;
                internal Field<ushort> DescIdx;
                internal Memory<UsedElement> Ring;
                
                internal UsedRing(Region r, int size)
                {
                    Flags = r.CreateField<ushort>(0);
                    DescIdx = r.CreateField<ushort>(2);
                    Ring = r.CreateMemory<UsedElement>(4, size / 8);
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
        
        internal class VirtioPciCommonCfg
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
        protected VirtioPciCommonCfg _common;
        protected QueueInfo _queueInfo;
        protected Region _notify;
        readonly private uint _notifyMultiplier;

        [StructLayout(LayoutKind.Sequential)]
        private struct Capability
        {
            PciDevice.Capability _cap;
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
                NotifyCfg = 2
            };
        }

        public VirtioPciDevice(PciDevice a)
        {
            _pci = a;

            var cap = _pci.CapabilitiesStart();
            do
            {
                if (cap.Span[0].Id == 0x09)
                {
                    var virtioCap = MemoryMarshal.CastMemory<PciDevice.Capability, Capability>(cap);
                    var bar = _pci.MapBar(virtioCap.Span[0].Bar);

                    // ignore IO bars
                    // there are legitimate usecases for them, but they're not useful *yet*
                    // (according to the virtio spec, they can be used for faster notifications on VMs)
                    if (bar.IsIo) continue;

                    var off = virtioCap.Span[0].Offset;
                    var len = virtioCap.Span[0].Length;
                    var type = virtioCap.Span[0].Type;

                    var slice = new Region(bar.Memory.Memory.Slice((int)off, (int)len));
                    if (type == Capability.CfgType.CommonCfg)
                    {
                        _common = new VirtioPciCommonCfg(slice);
                    }
                    else if (type == Capability.CfgType.NotifyCfg)
                    {
                        _notify = slice;
                        _notifyMultiplier = virtioCap.Span[0].NotifyOffMultiplier;
                    }
                }
            } while (_pci.CapabilitiesNext(ref cap));


            _common.DeviceStatus.Value = 0; // reset, not sure if needed
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

            for (int q = 0; q < 1; q++)
            {
                _common.QueueSelect.Value = (ushort)q;
                int size = _common.QueueSize.Value;
                // if the size is zero, the queue has to be ignored
                if (size == 0) continue;
                // TODO: support packed virtqueues instead
                _queueInfo = new(q, size, _notify.CreateField<ushort>(q * (int)_notifyMultiplier), _pci.GetMsix(0));
                _common.QueueDesc.Value = _queueInfo.DescPhys;
                _common.QueueDriver.Value = _queueInfo.AvailPhys;
                _common.QueueDevice.Value = _queueInfo.UsedPhys;
                _common.QueueMsixVector.Value = _queueInfo.Interrupt.Index;

                // and finally, enable the queue
                _common.QueueEnable.Value = 1;
            }

            // ready to work
            _common.DeviceStatus.Value |= VirtioPciCommonCfg.DevStatus.DriverOk;
        }
    }

    internal class VirtioBlockDevice : VirtioPciDevice
    {
        [StructLayout(LayoutKind.Sequential)]
        internal struct BlkReq
        {
            public uint Type;
            internal uint _0;
            public ulong Sector;
        }

        public VirtioBlockDevice(PciDevice a) : base(a)
        {
            Read(69);
        }

        void Read(ulong sector)
        {
            // start io
            var r = MemoryServices.AllocatePages(1);
            var rPhys = MemoryServices.GetPhys(r);
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
    }
}
