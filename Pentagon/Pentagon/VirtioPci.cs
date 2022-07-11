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
    public class VirtioPciDriver : IPciDriver
    {
        public bool Init(PciDevice a)
        {
            if (a.VendorId != 0x1AF4) return false;
            // Former is the transitional device ID, latter is the 1.0+ one
            if (a.DeviceId == 0x1001 || a.DeviceId == (0x1040 + 2))
            {
                new VirtioBlockDevice(a);    
                return true;
            }
            return false;
        }
    }

    internal class VirtioPciDevice
    {
        internal struct VirtioPciCommonCfg
        {
            // Whole device
            public Field<uint> DeviceFeatureSelect;
            public Field<uint> DeviceFeature;
            public Field<uint> DriverFeatureSelect;
            public Field<uint> DriverFeature;
            public Field<ushort> MsixConfig;
            public Field<ushort> NumQueues;
            public Field<byte> DeviceStatus;
            public Field<byte> ConfigGeneration;

            // Queue-specific: put a queue number in QueueSelect to get that queue's regs
            /*public Field<ushort> QueueSelect;
            public Field<ushort> QueueSize;
            public Field<ushort> QueueMsixVector;
            public Field<ushort> QueueEnable;
            public Field<ushort> QueueNotifyOff;
            public Field<ulong> QueueDesc;
            public Field<ulong> QueueDriver;
            public Field<ulong> QueueDevice;*/

            public VirtioPciCommonCfg(Region r)
            {
                DeviceFeatureSelect = r.CreateField<uint>(0);
                DeviceFeature = r.CreateField<uint>(4);
                DriverFeatureSelect = r.CreateField<uint>(8);
                DriverFeature = r.CreateField<uint>(12);
                MsixConfig = r.CreateField<ushort>(16);
                NumQueues = r.CreateField<ushort>(18);
                DeviceStatus = r.CreateField<byte>(20);
                ConfigGeneration = r.CreateField<byte>(21);
            }
        }
        protected PciDevice _pci;
        protected VirtioPciCommonCfg _common;
        

        const ushort CAP_CFG_TYPE = 3;
        const ushort CAP_BAR = 4;
        const ushort CAP_OFFSET = 8;
        const ushort CAP_LENGTH = 12;
        const byte TYPE_COMMON_CFG = 1;
        public VirtioPciDevice(PciDevice a)
        {
            _pci = a;

            var cap = _pci.Capabilities();
            while (true)
            {
                if (cap.CapabilityId == 0x09)
                {

                    byte type = cap.Read8(CAP_CFG_TYPE), bir = cap.Read8(CAP_BAR);
                    uint off = cap.Read8(CAP_OFFSET), len = cap.Read32(CAP_LENGTH);
                    var bar = _pci.MapBar(bir);
                    
                    // ignore IO bars
                    // there are legitimate usecases for them, but they're not useful *yet*
                    // (according to the virtio spec, they can be used for faster notifications on VMs)
                    if (bar.IsIo)
                    {
                        if (cap.Next()) continue;
                        else break;
                    }

                    var slice = new Region(bar.Memory.Memory.Slice((int)off, (int)len));
                    switch (type)
                    {
                        case TYPE_COMMON_CFG:
                        {
                            _common = new VirtioPciCommonCfg(slice);
                            break;
                        }
                    }
                }
                if (!cap.Next()) break;
            }

            _common.DeviceStatus.Value = 69;

        }
    }

    internal class VirtioBlockDevice : VirtioPciDevice
    {
        public VirtioBlockDevice(PciDevice a) : base(a)
        {

        }
    }
}
