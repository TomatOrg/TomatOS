using System;
using System.Runtime.InteropServices;

namespace Tomato.Hal.Acpi;

[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct AcpiCommonHeader
{
    public uint Signature;
    public uint Length;
}

[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct OemId
{
    public byte OemId1;
    public byte OemId2;
    public byte OemId3;
    public byte OemId4;
    public byte OemId5;
    public byte OemId6;

    public override string ToString()
    {
        return $"{(char)OemId1}{(char)OemId2}{(char)OemId3}{(char)OemId4}{(char)OemId5}{(char)OemId6}";
    }
    
}

[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct AcpiDescriptionHeader
{
    public uint Signature;
    public uint Length;
    public byte Revision;
    public byte Checksum;
    public OemId OemId;
    public ulong OemTableId;
    public uint OemRevision;
    public uint CreatorId;
    public uint CreatorRevision;
    
    public static string ToString(in AcpiDescriptionHeader header)
    {
        return $"v{header.Revision:x02} {header.OemId.ToString()} " +
               $"{(char)((byte)(header.OemTableId >> 0))}" +
               $"{(char)((byte)(header.OemTableId >> 8))}" +
               $"{(char)((byte)(header.OemTableId >> 16))}" +
               $"{(char)((byte)(header.OemTableId >> 24))}" +
               $"{(char)((byte)(header.OemTableId >> 32))}" +
               $"{(char)((byte)(header.OemTableId >> 40))}" +
               $"{(char)((byte)(header.OemTableId >> 48))}" +
               $"{(char)((byte)(header.OemTableId >> 56))}" +
               $" {header.OemRevision:x08} " +
               $"{(char)((byte)(header.CreatorId >> 0))}" +
               $"{(char)((byte)(header.CreatorId >> 8))}" +
               $"{(char)((byte)(header.CreatorId >> 16))}" +
               $"{(char)((byte)(header.CreatorId >> 24))}" +
               $" {header.CreatorRevision:x08}";
    }
}

[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct RootSystemDescriptorPointer
{
    
    public ulong Signature;
    public byte Checksum;
    public OemId OemId;
    public byte Revision; // was reserved in version 1
    public uint RsdtAddress;
    public uint Length;
    public ulong XsdtAddress;
    public byte ExtendedChecksum;
    public byte Reserved0;
    public byte Reserved1;
    public byte Reserved2;
}

[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct MultipleApicDescription
{
    public enum StructureType : byte
    {
        ProcessorLocalApic,
        IoApic,
        InterruptSourceOverride,
        NonMaskableInterruptSource,
        LocalApicNmi,
        LocalApicAddressOverride,
        IoSApic,
        ProcessorLocalSApic,
        PlatformInterruptSources,
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct IoApicStructure
    {
        public byte IoApicId;
        public byte Reserved;
        public uint IoApicAddress;
        public uint GlobalSystemInterruptBase;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct InterruptSourceOverrideStructure
    {
        public byte Bus;
        public byte Source;
        public uint GlobalSystemInterrupt;
        public ushort Flags;
    }
    
    public AcpiDescriptionHeader Header;
    public uint LocalApicAddress;
    public uint Flags;
    
}

[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct MemoryMappedEnhancedConfigurationSpaceBaseAddress 
{
    
    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct AllocationStructure
    {
        public ulong BaseAddress;
        public ushort PciSegmentGroupNumber;
        public byte StartBusNumber;
        public byte EndBusNumber;
        public uint Reserved;
    }

    public AcpiDescriptionHeader Header;
    public ulong Reserved;

}
