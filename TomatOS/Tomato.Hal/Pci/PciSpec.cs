using System;
using System.Runtime.InteropServices;

namespace Tomato.Hal.Pci;

public static class PciSpec
{
    
    public const int MaxBus = 255;
    public const int MaxDevice = 31;
    public const int MaxFunc = 7;

    public const byte HeaderLayoutCode = 0x7f;
    public const byte HeaderTypeMultiFunc = 0x80;
    public const byte HeaderTypeDevice = 0x00;
    public const byte HeaderTypePciToPciBridge = 0x01;

    public const int BaseAddressRegOffset = 0x10;
    public const int CapabilityPointerOffset = 0x34;

    public const int BirdgeSecondaryBusRegisterOffset = 0x19;

    public static string ClassCodeToString(byte classCode, byte subclassCode, byte progIf)
    {
        return classCode switch
        {
            0x00 => subclassCode switch
            {
                0x00 => "Non-VGA unclassified device",
                0x01 => "VGA compatible unclassified device",
                0x05 => "Image coprocessor",
                _ => "Unclassified device"  
            },
            0x01 => subclassCode switch
            {
                0x06 => progIf switch
                {
                    0x01 => "AHCI 1.0",
                    _ => "SATA controller"
                },
                0x08 => progIf switch
                {
                    0x01 => "NVMHCI",
                    0x02 => "NVM Express",
                    _ => "Non-Volatile memory controller"
                },
                _ => "Mass storage controller"
            },
            0x02 => subclassCode switch
            {
                0x00 => "Ethernet controller",
                _ => "Network controller"
            },
            0x03 => subclassCode switch
            {
                0x00 => "VGA compatible controller",
                0x01 => "XGA compatible controller",
                0x02 => "3D controller",
                _ => "Display controller"
            },
            0x04 => "Multimedia controller",
            0x05 => "Memory controller",
            0x06 => subclassCode switch
            {
                0x00 => "Host bridge",
                0x01 => "ISA bridge",
                0x02 => "PCI bridge",
                _ => "Bridge",
            },
            0x07 => "Communication controller",
            0x08 => "Generic system peripheral",
            0x09 => "Input device controller",
            0x0a => "Docking station",
            0x0b => "Processor",
            0x0c => subclassCode switch
            {
                0x03 => progIf switch
                {
                    0x00 => "UHCI",
                    0x10 => "OHCI",
                    0x20 => "EHCI",
                    0x30 => "XHCI",
                    0x40 => "USB4 Host Interface",
                    0xfe => "USB Device",
                    _ => "USB controller"
                },
                _ => "Serial bus controller"
            },
            0x0d => "Wireless controller",
            0x0e => "Intelligent controller",
            0x0f => "Satellite communications controller",
            0x10 => "Encryption controller",
            0x11 => "Signal processing controller",
            0x12 => "Processing accelerators",
            0x13 => "Non-Essential Instrumentation" ,
            0x40 => "Coprocessor" ,
            0xFF => "Unassigned class",
            _ => "<Unknown>"
        };
    }

    public static string PciCapabilityToStr(byte capId)
    {
        return capId switch
        {
            0x01 => "Power Management",
            0x02 => "Accelerated Graphics Port",
            0x03 => "Vital Product Data",
            0x04 => "Slot Identification",
            0x05 => "MSI",
            0x06 => "CompactPCI HotSwap",
            0x07 => "PCI-X",
            0x08 => "HyperTransport",
            0x09 => "Vendor-Specific",
            0x0A => "Debug port",
            0x0B => "CompactPCI Central Resource Control",
            0x0C => "PCI Standard Hot-Plug Controller",
            0x0D => "Bridge subsystem vendor/device ID",
            0x0E => "AGP Target PCI-PCI bridge",
            0x0F => "Secure Device",
            0x10 => "PCI Express",
            0x11 => "MSI-X",
            0x12 => "SATA Data/Index Conf.",
            0x13 => "PCI Advanced Features",
            0x14 => "PCI Enhanced Allocation",
            _ => $"{capId:x02}"
        };
    }
    
}


[Flags]
public enum PciCommand : ushort
{
    IoSpace = 1 << 0,
    MemorySpace = 1 << 1,
    BusMaster = 1 << 2,
    SpecialCycle = 1 << 3,
    MemoryWriteAndInvalidate = 1 << 4,
    VgaPaletteSnoop = 1 << 5,
    ParityErrorRespond = 1 << 6,
    SteppingControl = 1 << 7,
    Serr = 1 << 8,
    FastBackToBack = 1 << 9
}

[Flags]
public enum PciStatus : ushort
{   
    Capability = 1 << 4,
    FastBackToBackCapable = 1 << 7,
    MasterDataParityError = 1 << 8,
}

[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct PciHeader
{
    
    public ushort VendorId;
    public ushort DeviceId;
    public PciCommand Command;
    public PciStatus Status;
    public byte RevisionId;
    public byte ProgIf;
    public byte SubclassCode;
    public byte ClassCode;
    public byte CacheLineSize;
    public byte LatencyTimer;
    public byte HeaderType;
    public byte Bist;

    public bool IsPciDevice => (HeaderType & PciSpec.HeaderLayoutCode) == PciSpec.HeaderTypeDevice;
    public bool IsPciBridge => (HeaderType & PciSpec.HeaderLayoutCode) == PciSpec.HeaderTypePciToPciBridge;
    public bool IsPciMultiFunc => (HeaderType & PciSpec.HeaderTypeMultiFunc) != 0;
    
}
