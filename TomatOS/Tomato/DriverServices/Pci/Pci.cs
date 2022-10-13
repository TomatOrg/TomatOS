using System;
using System.Runtime.InteropServices;
using Tomato.Resources;

namespace Tomato.DriverServices.Pci;

/// <summary>
/// Implements the PCI scanning
/// </summary>
public static class Pci
{
    /// <summary>
    /// Get the slice of the whole ECAM for the single Bus:Device:Function.
    /// This ensures that the memory slice stored in PciDevice can never access memory 
    /// the caller doesn't have permission to access
    /// </summary>
    private static Memory<byte> GetConfigSpaceForDevice(Memory<byte> ecam, byte startBus, byte bus, byte dev, byte fn)
    {
        return ecam.Slice(((bus - startBus) << 20) + (dev << 15) + (fn << 12), 4096);
    }

    /// <summary>
    /// Register all PCI devices in the system
    /// TODO: log some stuff nicely :)
    /// </summary>
    internal static void Scan(Acpi.Acpi acpi)
    {
        var mcfg = acpi.FindTable(Acpi.Mcfg.Signature);
        var allocs = new Acpi.Mcfg(mcfg).Allocs.Span;

        // TODO: this ought to be the StartBus and EndBus values from allocs
        // but if the number of buses is near 256, it doesn't work on my (StaticSaga)'s machine
        // but I am not sure if the code is at fault
        byte startBus = 0;
        byte endBus = 1;
        
        // NOTE: McfgAllocation.EndBus is inclusive
        var phys = allocs[0].Base;
        var length = (endBus + 1 - startBus) << 20;
        var ecam = MemoryServices.Map(phys,  length);

        // iterate all the busses
        // TODO: convert to non-brute-force
        for (var bus = startBus; bus <= endBus; bus++)
        {
            // Iterate the devices on this bus 
            for (var device = (byte)0; device < 32; device++)
            {
                // get the slice and check if this is a valid pci device
                var ecamSlice = GetConfigSpaceForDevice(ecam, startBus, bus, device, 0);
                var vendorId = MemoryMarshal.Read<ushort>(ecamSlice.Span);
                if (vendorId == 0xFFFF)
                    continue;

                // the device is valid, create it 
                var pciDev = new PciDevice(bus, device, 0, ecamSlice);

                // the device is valid, add it for drivers to see
                ResourceManager<PciDevice>.Add(pciDev);

                // Skip functions if there are no functions for the device
                if ((pciDev.ConfigHeader.HeaderType & 0x80) == 0) 
                    continue;

                // iterate the functions on this device
                for (byte func = 1; func < 8; func++)
                {
                    // Same as the main device
                    ecamSlice = GetConfigSpaceForDevice(ecam, startBus, bus, device, func);
                    vendorId = MemoryMarshal.Read<ushort>(ecamSlice.Span);
                    if (vendorId == 0xFFFF)
                        continue;

                    // valid, create it 
                    pciDev = new PciDevice(bus, device, func, ecamSlice);

                    // and register...
                    ResourceManager<PciDevice>.Add(pciDev);
                }
            }
        }
    }

}

/// <summary>
/// Fields common to all PCI capabilities
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public struct PciCapability
{
    public byte Id;
    public byte Next;

    /// <summary>
    /// The layout of an MSI-X capability
    /// </summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct Msix
    {
        public PciCapability Header;
        public MsgCtrl MessageControl;
        public uint Table;
        public uint Pending;

        [Flags]
        public enum MsgCtrl : ushort
        {
            TableSizeMask = 0b1111111111,
            GlobalMask = (ushort)(1u << 14),
            Enable = (ushort)(1u << 15)
        }
    }
}

/// <summary>
/// Represents the PCI config space header that exists per device
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public struct PciConfigHeader
{
    public ushort VendorId;
    public ushort DeviceId;
    public CommandBits Command;
    public ushort Status;
    public byte RevId;
    public byte ProgIf;
    public byte Subclass;
    public byte ClassCode;
    public byte CacheLineSize;
    public byte LatencyTimer;
    public byte HeaderType;
    public byte Bist;
    
    public enum CommandBits : ushort
    {
        INTxDisable = (ushort)(1u << 10)
    }
}
