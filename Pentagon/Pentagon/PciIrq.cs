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
    static public class Pci
    {
        public class Msix
        {
            ushort _allocated;
            Memory<MsixEntry> _msix;

            public Msix(PciDevice d)
            {
                _allocated = 0;
                _msix = null;

                var cap = d.CapabilitiesStart();
                do
                {
                    if (cap.Span[0].Id == 0x11)
                    {
                        var msixCap = MemoryMarshal.CastMemory<PciDevice.Capability, Capability>(cap);
                        var table = msixCap.Span[0].Table;
                        var tableOff = table & (~0b111ul);
                        byte tableBir = (byte)(table & 0b111ul);

                        // TODO: io bar check ig
                        var bar = d.MapBar(tableBir);

                        // TODO: get length and dont hard-map 1 entry
                        var tableRegion = new Region(bar.Memory.Memory.Slice((int)tableOff, 16));

                        _msix = tableRegion.CreateMemory<MsixEntry>(0, 1);

                        // disable legacy IRQs 
                        d.Command |= PciDevice.CommandBits.INTxDisable;

                        // enable MSIX
                        // NOTE: handle global masking properly
                        msixCap.Span[0].MessageControl |= Capability.MsgCtrl.Enable;

                        break;
                    }
                } while (d.CapabilitiesNext(ref cap));
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
                    var m = Msi.GetData(IrqNum, 0);
                    _ent.Span[0].Addr = m.Addr;
                    _ent.Span[0].Data = (uint)m.Data;
                    _ent.Span[0].Ctrl = 1; // masked
                }
            }


            [StructLayout(LayoutKind.Sequential)]
            private struct Capability
            {
                PciDevice.Capability _cap;
                public MsgCtrl MessageControl;
                public uint Table;
                public uint Pending;

                public enum MsgCtrl : ushort
                {
                    Enable = (ushort)(1u << 15)
                };
            }

            [StructLayout(LayoutKind.Sequential)]
            public struct MsixEntry
            {
                public ulong Addr;
                public uint Data;
                public uint Ctrl;
            }
        }
    }
}
