using System;
using System.Diagnostics;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Tomato.Hal.Acpi.Resource;
using Tomato.Hal.Drivers.Ps2;
using Tomato.Hal.Pci;
using Tomato.Hal.Platform.Pc;

namespace Tomato.Hal.Acpi;

public static class AcpiManager
{

    private static uint Signature(char a, char b, char c, char d)
    {
        return a | ((uint)b << 8) | ((uint)c << 16) | ((uint)d << 24);
    }

    /// <summary>
    /// Handle APICs, simply adds ISOs and IoApics to the system.
    /// </summary>
    private static void HandleApic(Memory<byte> table)
    {
        var iter = new SpanIterator(table.Span);
        ref var header = ref iter.Get<MultipleApicDescription>();
        while (iter.Left != 0)
        {
            var type = iter.Get<MultipleApicDescription.StructureType>();
            var length = iter.Get<byte>();
            switch (type)
            {
                // found a new IoApic, add it 
                case MultipleApicDescription.StructureType.IoApic:
                {
                    ref var ioapic = ref iter.Get<MultipleApicDescription.IoApicStructure>();
                    var ioa = new IoApic(ioapic.IoApicAddress, ioapic.GlobalSystemInterruptBase);
                    Debug.Print($"IOAPIC[{IoApic.IoApics.Count}]: id={ioapic.IoApicId}, address={ioa.Address:x8}, gsi={ioa.GsiBase}-{ioa.GsiEnd}");
                    IoApic.IoApics.Add(ioa);
                } break;

                // found a new ISO, add it 
                case MultipleApicDescription.StructureType.InterruptSourceOverride:
                {
                    ref var iso = ref iter.Get<MultipleApicDescription.InterruptSourceOverrideStructure>();
                    Debug.Print($"ACPI: ISO (bus={iso.Bus}, irq={iso.Source}, gsi={iso.GlobalSystemInterrupt})");
                    IoApic.Isos.Add(iso);
                } break;
                
                default:
                    // skip whatever that was left
                    iter.Skip<byte>(length - 2);
                    break;
            }
        }
    }
    
    /// <summary>
    /// Handle MCFG, just starts PCI scan based on the mappings
    /// </summary>
    /// <param name="table"></param>
    private static void HandleMcfg(Memory<byte> table)
    {
        var iter = new SpanIterator(table.Span);
        ref var header = ref iter.Get<MemoryMappedEnhancedConfigurationSpaceBaseAddress>();
        while (iter.Left != 0)
        {
            var alloc = iter.Get<MemoryMappedEnhancedConfigurationSpaceBaseAddress.AllocationStructure>();
            if (alloc.PciSegmentGroupNumber != 0 || alloc.StartBusNumber != 0)
            {
                throw new NotSupportedException("TODO: better PCI support");
            }

            // map the whole ecam properly
            var ecam = MemoryServices.Map(alloc.BaseAddress, (alloc.EndBusNumber - alloc.StartBusNumber + 1) * 4096);
            new PciManager.PciScan(ecam).Scan();
        }
    }
    
    /// <summary>
    /// Called for each apic table present in the system, checks if its something we wanna
    /// handle and if so we handle it
    /// </summary>
    private static void ProcessTable(ulong ptr)
    {
        var header = MemoryServices.Map<AcpiDescriptionHeader>(ptr).Span[0];
        var table = MemoryServices.Map(ptr, (int)header.Length);

        Debug.Print($"ACPI: " +
                    $"{(char)((byte)(header.Signature >> 0))}" +
                    $"{(char)((byte)(header.Signature >> 8))}" +
                    $"{(char)((byte)(header.Signature >> 16))}" +
                    $"{(char)((byte)(header.Signature >> 24))}" +
                    $" {ptr:x016} {header.Length:x08} ({AcpiDescriptionHeader.ToString(header)})");
        
        // and now print it 
        if (header.Signature == Signature('A', 'P', 'I', 'C'))
        {
            HandleApic(table);
        } else if (header.Signature == Signature('M', 'C', 'F', 'G'))
        {
            HandleMcfg(table);
        }
    }

    /// <summary>
    /// Iterate all the ACPI tables
    /// </summary>
    private static void IterateTables()
    {
        var phys = Hal.GetRsdp();
        var rsdp = MemoryServices.Map<RootSystemDescriptorPointer>(phys).Span[0];
        
        if (rsdp.Revision >= 0x02)
        {
            // revision 2, we have 64bit pointers
            var header = MemoryServices.Map<AcpiDescriptionHeader>(rsdp.XsdtAddress).Span[0];
            var count = (header.Length - Unsafe.SizeOf<AcpiDescriptionHeader>()) / Unsafe.SizeOf<ulong>();
            var tables = MemoryServices.Map<ulong>(rsdp.XsdtAddress + (ulong)Unsafe.SizeOf<AcpiDescriptionHeader>(), (int)count).Span;

            Debug.Print($"ACPI: RSDP {phys:x016} {rsdp.Length:x08} (v{rsdp.Revision:x02} {rsdp.OemId})");
            Debug.Print($"ACPI: XSDT {phys:x016} ({AcpiDescriptionHeader.ToString(header)})");

            foreach (var table in tables)
            {
                ProcessTable(table);
            }
        }
        else
        {
            // revision 1, we have 32bit pointers
            var header = MemoryServices.Map<AcpiDescriptionHeader>(rsdp.RsdtAddress).Span[0];
            var count = (header.Length - Unsafe.SizeOf<AcpiDescriptionHeader>()) / Unsafe.SizeOf<uint>();
            var tables = MemoryServices.Map<uint>(rsdp.RsdtAddress + (ulong)Unsafe.SizeOf<AcpiDescriptionHeader>(), (int)count).Span;

            Debug.Print($"ACPI: RSDP {phys:x016} (v00 {rsdp.OemId})");
            Debug.Print($"ACPI: RSDT {phys:x016} ({AcpiDescriptionHeader.ToString(header)})");

            foreach (var table in tables)
            {
                ProcessTable(table);
            }
        }
    }
    
    /// <summary>
    /// Do ACPI initialization
    /// </summary>
    internal static void Init()
    {
        // starts by iterating the tables
        IterateTables();
        
        // TODO: startup the AML interpreter
        
        // TODO: enable ACPI mode
        
        // for now create PS2 resources
        InitPs2();
    }

    internal static void InitPs2()
    {
        // create all the resources
        var dataPort = new IoResource(0x60, 1);
        var commandPort = new IoResource(0x64, 1);
        var keyboardIrq = new IrqResource(new[] { IoApic.RegisterIrq(1) });
        var mouseIrq = new IrqResource(new[] { IoApic.RegisterIrq(12) });

        // init the controller        
        new Ps2Controller(commandPort, dataPort, keyboardIrq, mouseIrq);
    }
    
}