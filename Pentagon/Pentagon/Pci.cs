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
        private Pci.Msix _msix = null;
        /// <summary>
        /// Get the slice of the whole ECAM for the single Bus:Device:Function.
        /// This ensures that the memory slice stored in PciDevice can never access memory 
        /// the caller doesn't have permission to access
        /// </summary>
        public static Region GetEcamSlice(byte startBus, Region ecam, byte bus, byte dev, byte fn)
            => ecam.CreateRegion(((bus - startBus) << 20) + (dev << 15) + (fn << 12), 4096);
        
        public PciDevice(byte bus, byte dev, byte fn, Region ecamSlice)
        {
            Bus = bus;
            Device = dev;
            Function = fn;
            EcamSlice = ecamSlice;
        }

        public Bar MapBar(byte bir)
            => new Bar(this, bir);

        public ushort VendorId => Read16(0);
        public ushort DeviceId => Read16(2);
        public uint Command { get => Read32(4); set => Write32(4, value); }
        public ushort HeaderType => Read16(14);

        public Pci.Msix.Irq GetMsix(int core) => (_msix ??= new(this)).Allocate(core);

        /// <summary>
        /// Get PCI capability linked list 
        /// </summary>
        /// <returns>First capability</returns>
        // TODO: check if capabilities are supported at all
        public Memory<Capability> CapabilitiesStart()
        {
            var idx = Read8(0x34);
            return EcamSlice.CreateMemory<Capability>(idx, 1);
        }
        public bool CapabilitiesNext(ref Memory<Capability> c)
        {
            var idx = c.Span[0].Next;
            if (idx == 0) return false;
            c = EcamSlice.CreateMemory<Capability>(idx, 1);
            return true;
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
            internal IMemoryOwner<byte> Memory;
            
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
        /// Fields common to all PCI capabilities
        /// </summary>
        [StructLayout(LayoutKind.Sequential)]
        public struct Capability
        {
            public byte Id;
            public byte Next;
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

            // TODO: this ought to be the StartBus and EndBus values from allocs
            // but if the number of buses is near 256, it doesn't work on my (StaticSaga)'s machine
            // but I am not sure if the code is at fault
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
            => _drivers.Add(driver);
        
        /// <summary>
        /// Scans all PCI buses 
        /// </summary>
        internal void Scan()
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
