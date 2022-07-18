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
    /// PCI function accessor. It offers manageable access to the config space
    /// </summary>
    public class PciDevice
    {
        public byte Bus;
        public byte Device;
        public byte Function;
        public Region EcamSlice;
        private Msix _msix = null;
        
        /// <summary>
        /// Get the slice of the whole ECAM for the single Bus:Device:Function.
        /// This ensures that the memory slice stored in PciDevice can never access memory 
        /// the caller doesn't have permission to access
        /// </summary>
        public static Region GetEcamSlice(byte startBus, Region ecam, byte bus, byte dev, byte fn)
        {
            return ecam.CreateRegion(((bus - startBus) << 20) + (dev << 15) + (fn << 12), 4096);
        }
        
        public PciDevice(byte bus, byte dev, byte fn, Region ecamSlice)
        {
            Bus = bus;
            Device = dev;
            Function = fn;
            EcamSlice = ecamSlice;
        }
        
        public ushort VendorId => Read16(0);
        public ushort DeviceId => Read16(2);
        public uint Command { get => Read32(4); set => Write32(4, value); }
        public ushort HeaderType => Read16(14);

        public Bar MapBar(byte bir)
        {
            return new Bar(this, bir);
        }

        public Msix.Irq GetMsix(int core)
        {
            if (_msix == null) _msix = new(this);
            return _msix.Allocate(core);
        }
        /// <summary>
        /// Get PCI capability linked list 
        /// </summary>
        /// <returns>First capability</returns>
        // TODO: check if capabilities are supported at all
        public Capability Capabilities() => new Capability(this);

        internal class MsiData
        {
            public ulong Data;
            public ulong Addr;
        };

        internal static MsiData GetMsiData(ulong irqNum, int core)
        {
            MsiData m = new()
            {
                Data = irqNum,
                Addr = 0xFEE00000ul | ((ulong)core << 12)
            };
            return m;
        }

        public class Msix
        {
            ushort _allocated;
            Memory<MsixEntry> _msix;

            public Msix(PciDevice d)
            {
                _allocated = 0;
                _msix = null;

                var cap = d.Capabilities();
                while (true)
                {
                    if (cap.CapabilityId == 0x11)
                    {
                        var table = cap.Read32(4);
                        var tableOff = table & (~0b111ul);
                        byte tableBir = (byte)(table & 0b111ul);

                        // TODO: io bar check ig
                        var bar = d.MapBar(tableBir);

                        // TODO: get length and dont hard-map 1 entry
                        var tableRegion = new Region(bar.Memory.Memory.Slice((int)tableOff, 16));

                        _msix = tableRegion.CreateMemory<MsixEntry>(0, 1);

                        // disable legacy IRQs 
                        d.Command |= 1u << 10;

                        // enable MSIX
                        // NOTE: handle global masking properly
                        cap.Write16(2, (ushort)(cap.Read16(2) | (1u << 15)));

                        break;
                    }
                    if (!cap.Next()) break;
                }
            }

            public Irq Allocate(int core)
            {    
                var i = new Irq(_msix, _allocated);
                _allocated++;
                return i;
            }

            public class Irq : HAL.Irq
            {
                Memory<MsixEntry> _ent;

                public ushort Index;

                internal Irq(Memory<MsixEntry> ent, ushort index) : base(MemoryMarshal.CastMemory<MsixEntry, uint>(ent).Slice(3))
                {
                    _ent = ent;
                    Index = index;
                    MsiData m = GetMsiData(IrqNum, 0);
                    _ent.Span[0].Addr = m.Addr;
                    _ent.Span[0].Data = (uint)m.Data;
                    _ent.Span[0].Ctrl = 1; // masked
                }
            }

            [StructLayout(LayoutKind.Sequential)]
            public struct MsixEntry
            {
                public ulong Addr;
                public uint Data;
                public uint Ctrl;
            }
        }        
        
        // TODO: optimize these functions
        public byte Read8(int offset)
        {
            var p = EcamSlice.Span.Slice(offset);
            return p[0];
        }

        public ushort Read16(int offset)
        {
            var p = MemoryMarshal.Cast<byte, ushort>(EcamSlice.Span.Slice(offset));
            return p[0];
        }

        public uint Read32(int offset)
        {
            var p = MemoryMarshal.Cast<byte, uint>(EcamSlice.Span.Slice(offset));
            return p[0];
        }

        public void Write16(int offset, ushort value)
        {
            var p = MemoryMarshal.Cast<byte, ushort>(EcamSlice.Span.Slice(offset));
            p[0] = value;
        }

        public void Write32(int offset, uint value)
        {
            var p = MemoryMarshal.Cast<byte, uint>(EcamSlice.Span.Slice(offset));
            p[0] = value;
        }

        /// <summary>
        /// Class representing a base address register, whic can be either IO ports or memory.
        /// </summary>
        public class Bar
        {
            public readonly bool IsIo;
            public IMemoryOwner<byte> Memory;
            
            internal Bar(PciDevice a, byte bir)
            {
                if (bir > 5)
                {
                    throw new IndexOutOfRangeException();
                }
                var off = 16 + bir * 4;
                var bar = a.Read32(off);
                IsIo = (bar & 1) > 0;
                if (IsIo)
                {
                    // TODO
                }
                else
                {
                    ulong address = bar & (~0xFul);
                    // Calculate the length
                    a.Write32(off, ~0u);
                    ulong length = ~(a.Read32(off) & (~0xFu)) + 1;
                    a.Write32(off, bar);

                    // If it's a 64-bit memory BAR
                    if (((address >> 1) & 0b11) == 2)
                    {
                        off += 4; // go to the next BAR
                        ulong higherHalf = a.Read32(off) & (~0xFu);
                        address |= higherHalf << 32;
                        // TODO: 64bit length
                    }
                    Memory = HAL.MemoryServices.MapPages(address, (int)(length / 4096));
                }
            }
        }

        /// <summary>
        /// Class representing a capability with iteration to the next one
        /// </summary>
        public class Capability
        {
            private readonly PciDevice _addr;
            private ushort _off;

            internal Capability(PciDevice a)
            {
                _addr = a;
                _off = Read8(0x34);
            }

            public bool Next()
            {
                ushort next = Read8(1);
                if (next == 0) return false;
                _off = next;
                return true;
            }

            public byte CapabilityId { get => Read8(0); }
            public byte Read8(ushort offset) => _addr.Read8((ushort)(_off + offset));
            public ushort Read16(ushort offset) => _addr.Read16((ushort)(_off + offset));
            public uint Read32(ushort offset) => _addr.Read32((ushort)(_off + offset));

            public void Write16(ushort offset, ushort value) => _addr.Write16((ushort)(_off + offset), value);

        }
    }

    public class PciRoot
    {
        // TODO: this as a whole only supports the first segment
        // but hardware that has more than one is rare anyways
        private readonly IMemoryOwner<byte> _ecam;
        private readonly byte _startBus, _endBus;
        private List<IPciDriver> _drivers = new();

        /// <summary>
        /// PCI management singleton: uses ACPI for ECAM finding.
        /// </summary>
        public PciRoot(Acpi acpi)
        {
            var mcfg = acpi.FindTable(Acpi.Mcfg.Signature);
            var allocs = mcfg.AsSpan<Acpi.Mcfg.McfgAllocation>(44, 1);

            _startBus = 0;
            _endBus = 1;
            var phys = allocs[0].Base;
            var length = ((int)(_endBus + 1) - _startBus) << 20; // NOTE: McfgAllocation.EndBus is inclusive
            _ecam = HAL.MemoryServices.MapPages(phys, length / 4096);
        }

        /// <summary>
        /// Add driver to supported list
        /// </summary>
        public void AddDriver(IPciDriver driver)
        {
            _drivers.Add(driver);
        }
        
        /// <summary>
        /// Scans all PCI buses 
        /// </summary>
        public void Scan()
        {
            for (byte b = _startBus; b <= _endBus; b++)
            {
                for (byte d = 0; d < 32; d++)
                {
                    var devAddr = new PciDevice(b, d, 0, PciDevice.GetEcamSlice(_startBus, new Region(_ecam.Memory), b, d, 0));
                    if (devAddr.VendorId == 0xFFFF) continue;
                    var functions = ((devAddr.HeaderType & 0x80) > 0) ? 8 : 1;
                    for (byte f = 0; f < functions; f++)
                    {
                        var fnAddr = new PciDevice(b, d, f, PciDevice.GetEcamSlice(_startBus, new Region(_ecam.Memory), b, d, f));
                        if (fnAddr.VendorId == 0xFFFF) continue;
                        SearchDevice(fnAddr);
                    }
                }
            }
        }

        private void SearchDevice(PciDevice a)
        {
            foreach (var drv in _drivers)
            {
                var success = drv.Init(a);
                if (success) break;
            }
        }
    }
}
