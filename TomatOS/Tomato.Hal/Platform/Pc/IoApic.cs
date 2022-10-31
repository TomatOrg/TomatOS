using System.Collections.Generic;
using Tomato.Hal.Acpi;

namespace Tomato.Hal.Platform.Pc;

internal class IoApic
{

    internal static readonly List<IoApic> IoApics = new();
    internal static readonly List<MultipleApicDescription.InterruptSourceOverrideStructure> Isos = new();

    /// <summary>
    /// Get the IoApic that handles the given GSI
    /// </summary>
    private static IoApic GetFromGsi(uint gsi)
    {
        foreach (var ioapic in IoApics)
        {
            if (gsi >= ioapic.GsiBase && gsi < ioapic.GsiEnd)
            {
                return ioapic;
            }
        }

        return null;
    }

    /// <summary>
    /// Register a new IRQ, returning an Irq class that can be used to wait for the interrupt
    /// </summary>
    internal static Irq RegisterIrq(uint irqNum)
    {
        // assume 1:1 mapping if no ISO
        var gsi = irqNum;
        ushort flags = 0;
        foreach (var iso in Isos)
        {
            if (iso.Source != irqNum) 
                continue;
            
            gsi = iso.GlobalSystemInterrupt;
            flags = iso.Flags;
            break;
        }
        
        // get the IoApic for this irq
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

    public ulong Address { get; }

    private Field<uint> _ioRegSel;
    private Field<uint> _ioRegWin;

    public uint GsiBase { get; }
    public uint GsiEnd { get; }

    public uint IoApicVer => Read(1);
    
    internal IoApic(uint address, uint gsiBase)
    {
        // map the io apic so we can access it 
        var region = new Region(MemoryServices.Map(address, MemoryServices.PageSize));
        _ioRegSel = region.CreateField<uint>(0);
        _ioRegWin = region.CreateField<uint>(16);
        
        // now set the fields
        Address = address;
        GsiBase = gsiBase;
        GsiEnd = gsiBase + ((IoApicVer >> 16) & 0xFF);
    }
    
    public void IoRedTbl(uint i, ulong value)
    {
        Write(0x10 + i * 2, (uint)(value & 0xFFFFFFFF));
        Write(0x11 + i * 2, (uint)(value >> 32));
    }
    
    public uint Read(uint idx)
    {
        _ioRegSel.Value = idx;
        return _ioRegWin.Value;
    }

    public void Write(uint idx, uint data)
    {
        _ioRegSel.Value = idx;
        _ioRegWin.Value = data;
    }

}