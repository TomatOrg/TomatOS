using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using Tomato.Resources;

namespace Tomato.DriverServices;

internal static class IoApic
{
    static List<IoApicData> _ioApics = new(16);
    static List<Acpi.Madt.Iso> _isos;
    internal static void Scan(Acpi.Acpi acpi)
    {
        var madtMem = acpi.FindTable(Acpi.Madt.Signature);
        var madt = new Acpi.Madt(madtMem);
        _isos = madt.Isos;

        foreach (var ioapic in madt.IoApics)
        {
            Log.LogString("IOAPIC Id=");
            Log.LogHex(ioapic.Id);
            Log.LogString(" Address=");
            Log.LogHex(ioapic.Address);
            Log.LogString(" GsiBase=");
            Log.LogHex(ioapic.GsiBase);
            Log.LogString("\n");

            _ioApics.Add(new IoApicData(ioapic));
        }

        foreach (var iso in _isos)
        {
            Log.LogString("ISO BusSource=");
            Log.LogHex(iso.BusSource);
            Log.LogString(" IrqSource=");
            Log.LogHex(iso.IrqSource);
            Log.LogString(" Gsi=");
            Log.LogHex(iso.Gsi);
            Log.LogString("\n");
        }
    }

    internal static Irq RegisterIrq(uint irqNum)
    {
        uint gsi = irqNum; // assume 1:1 mapping if no ISOs
        ushort flags = 0;
        foreach (var iso in _isos)
        {
            if (iso.IrqSource == irqNum)
            {
                gsi = iso.Gsi;
                flags = iso.Flags;
                break;
            }
        }        
        var ioapic = GetFromGsi(gsi);
        var idx = gsi - ioapic.GsiBase;
        var vector = Irq.AllocateIrq(1, Irq.IrqMaskType.IoApic, ioapic.Address | idx);

        ulong redirVal = 0;
        redirVal |= (uint)vector;
        redirVal |= 0b001u << 8; // lowest priority mode
        redirVal |= 1ul << 16; // like MSI(X), masked by default
        if ((flags & (1u << 1)) != 0) redirVal |= 1u << 13; // pin polarity, 0 = active high, 1 = low
        // THIS IS VERY IMPORTANT
        // IF YOU GET AN EDGE TRIGGERED IOAPIC IRQ WHILE IT'S MASKED, YOU WON'T GET IT
        // SO PUT IT AS LEVEL TRIGGERED TO AVOID LOSING IRQS
        redirVal |= 1u << 15; // 0 = edge triggered, 1 = level triggered
        ioapic.IoRedTbl(idx, redirVal);
        return new Irq(vector);
    }

    // FIXME: for some weird reason if I return directly from the if, it doesn't work. TDN bug
    static internal IoApicData GetFromGsi(uint gsi)
    {
        IoApicData data = null;
        foreach (var ioapic in _ioApics)
        {
            if (gsi >= ioapic.GsiBase && gsi < ioapic.GsiEnd)
            {
                data = ioapic;
            }
        }
        return data;
    }

    internal class IoApicData
    {
        internal ulong Address;
        internal Field<uint> IoRegSel;
        internal Field<uint> IoRegWin;
        internal uint GsiBase = 0;
        internal uint GsiEnd = 0;
        internal IoApicData(Acpi.Madt.IoApic i)
        {
            Address = i.Address;
            var m = MemoryServices.Map(Address, MemoryServices.PageSize);
            var r = new Region(m);
            IoRegSel = new(r, 0);
            IoRegWin = new(r, 16);

            GsiBase = i.GsiBase;
            GsiEnd = i.GsiBase + ((IoApicVer >> 16) & 0xFF);
        }

        internal uint IoApicVer => read(1);
        internal void IoRedTbl(uint i, ulong value) {
            write(0x10 + i * 2, (uint)(value & 0xFFFFFFFF));
            write(0x11 + i * 2, (uint)(value >> 32));
        }

        uint read(uint idx)
        {
            IoRegSel.Value = idx;
            return IoRegWin.Value;
        }
        void write(uint idx, uint data)
        {
            IoRegSel.Value = idx;
            IoRegWin.Value = data;
        }
    }

}
