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
                        ref var msixCap = ref MemoryMarshal.CastMemory<PciDevice.Capability, Capability>(cap).Span[0];
                        var table = msixCap.Table;
                        var tableOff = table & (~0b111ul);
                        var tableBir = (byte)(table & 0b111ul);
                        var tableSize = (int)(msixCap.MessageControl & Capability.MsgCtrl.TableSizeMask) + 1;
                        
                        // TODO: io bar check ig
                        var bar = d.MapBar(tableBir);
                        var tableRegion = new Region(bar.Memory.Memory.Slice((int)tableOff, tableSize * 16));
                        _msix = tableRegion.CreateMemory<MsixEntry>(0, tableSize);

                        // disable legacy IRQs 
                        d.Config.Command |= PciDevice.CommandBits.INTxDisable;

                        // enable MSIX
                        msixCap.MessageControl = (msixCap.MessageControl | Capability.MsgCtrl.Enable) & (~Capability.MsgCtrl.GlobalMask);

                        break;
                    }
                } while (d.CapabilitiesNext(ref cap));
            }

            internal Irq Allocate(int core)
            {
                var i = new Irq(_msix, _allocated);
                _allocated++;
                return i;
            }

            internal class Irq : HAL.Irq
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
                    TableSizeMask = 0b1111111111,
                    GlobalMask = (ushort)(1u << 14),
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
