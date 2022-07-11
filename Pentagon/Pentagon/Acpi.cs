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
    /// ACPI management singleton
    /// </summary>
    public class Acpi
    {
        internal Region[] _pointers;
        
        public Acpi()
        {
            var rsdtPhys = GetRsdt();
            var region = new Region(MemoryServices.Map(rsdtPhys, 8192).Memory); // TODO: allocate as much as Length wants
            var rsdt = new Rsdt(region);
            var count = (int)(rsdt.DHdr.Length.Value - 36) / 4;
            var p = region.AsSpan<uint>(36, count);
            _pointers = new Region[count];
            for (int i = 0; i < count; i++)
            {
                var rgn = new Region(MemoryServices.Map(p[i], 8192).Memory);
                _pointers[i] = rgn;
            }
        }

        public Region FindTable(uint signature)
        {
            for (int i = 0; i < _pointers.Length; i++)
            {
                var mem = _pointers[i];
                var sig = MemoryMarshal.Cast<byte, uint>(mem.Span)[0];
                if (sig == signature)
                {
                    return mem;
                }
            }
            return null;
        }


        #region Native functions
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern ulong GetRsdt();
        
        #endregion


        #region ACPI tables
        
        public class DescriptorHeader
        {
            internal Field<uint> Signature;
            internal Field<uint> Length;
            public DescriptorHeader(Region r)
            {
                Signature = r.CreateField<uint>(0);
                Length = r.CreateField<uint>(4);
            }
        }

        public class Rsdt
        {
            internal DescriptorHeader DHdr;
            public Rsdt(Region r)
            {
                DHdr = new(r);
            }
        }

        public class Mcfg
        {
            public const uint Signature = 0x4746434D;
            internal DescriptorHeader DHdr;
            internal Memory<McfgAllocation> Allocs;
            public Mcfg(Region r)
            {
                DHdr = new(r);
                int allocations = (int)((DHdr.Length.Value - 44) / 16);
                Allocs = r.CreateMemory<McfgAllocation>(44, allocations);
            }

            [StructLayout(LayoutKind.Sequential)]
            public struct McfgAllocation
            {
                public ulong Base;
                public ushort Segment;
                public byte StartBus;
                public byte EndBus;
                private uint _0;
            }
        }
        
        #endregion
    }
}