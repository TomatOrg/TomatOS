using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Numerics;
using System.Runtime.InteropServices;

namespace Tomato.Hal.Pci;

public class PciDevice
{

    public int Bus { get; }
    public int Device { get; }
    public int Function { get; }

    public Memory<byte> Config { get; }

    private Field<PciHeader> _header;

    public ref PciHeader Header => ref _header.Value;

    public ushort VendorId => Header.VendorId;
    public ushort DeviceId => Header.DeviceId;

    public byte ClassCode => Header.ClassCode;
    public byte SubclassCode => Header.SubclassCode;
    public byte ProgIf => Header.ProgIf;

    /// <summary>
    /// The MSI-X configuration for this device, will be null if MSI-X
    /// is not supported
    /// </summary>
    public Msix Msix { get; private set; }
    
    /// <summary>
    /// The base addresses of the device, there
    /// are always the same number of bars as the device should have (so
    /// 6 on normal devices and 2 on bridges). Empty entries mean that there
    /// was nothing relevant at the bar.
    ///
    /// NOTE: this does not include I/O Port bars.
    /// </summary>
    public Memory<byte>[] MemoryBars { get; private set; }

    // The capabilities of the device, taken from the start to the
    // end of the old region
    public Memory<byte>[] Capabilities { get; private set; }

    internal PciDevice(Memory<byte> config, int bus, int dev, int func)
    {
        Bus = bus;
        Device = dev;
        Function = func;
        Config = config;

        var region = new Region(config);
        _header = region.CreateField<PciHeader>(0);
        
        // print about the device
        Debug.Print($"{Bus:x02}:{Device:x02}.{Function:x}: {PciSpec.ClassCodeToString(ClassCode, SubclassCode, ProgIf)}: {VendorId:x04}:{DeviceId:x04} (rev {Header.RevisionId:x02})");
        
        // initialize it 
        SetMemoryBars();
        SetCapabilities();
    }

    private void SetMemoryBars()
    {
        // figure the bar count 
        var barCount = 0;
        if (Header.IsPciDevice)
        {
            barCount = 6;
        } else if (Header.IsPciBridge)
        {
            barCount = 2;
        }
        MemoryBars = new Memory<byte>[barCount];

        // get just the bars as a span
        var bars = MemoryMarshal.Cast<byte, uint>(Config.Span.Slice(PciSpec.BaseAddressRegOffset)).Slice(0, barCount);
        for (var i = 0; i < bars.Length; i++)
        {
            var idx = i;
            
            // check the bar
            var orig = bars[i];
            bars[i] = 0xFFFFFFFF;
            var value = bars[i];
            bars[i] = orig;
            
            // not implemented
            if (value == 0)
                continue;

            // figure the length, address and type
            var length = 0UL;
            var addr = 0UL;
            var mem = false;
            var mem64 = false;
            if ((value & 0x01) != 0)
            {
                const uint mask = 0xfffffffcU;
                if ((value & 0xFFFF0000) != 0)
                {
                    // IO32 bar 
                    length = ((~(value & mask)) + 1);
                    
                }
                else
                {
                    // IO16 bar
                    length = 0x0000FFFF & ((~(value & mask)) + 1);
                }

                // Some platforms implement IO bar with 0 length, 
                // need to treat as no-bar
                if (length == 0)
                    continue;

                addr = orig & mask;
            }
            else
            {
                const uint mask = 0xfffffff0;
                addr = orig & mask;

                mem = true;

                switch (value & 0x7)
                {
                    // mem32 bar
                    case 0x00:
                        length = (~(value & mask)) + 1;
                        break;
                        
                    // mem64 bar
                    case 0x04:
                        mem64 = true;
                        
                        // will be combined with the higher half
                        length = value & mask;
                        i++;

                        // check the high part of the 64bit address
                        orig = bars[i];
                        bars[i] = 0xFFFFFFFF;
                        value = bars[i];
                        bars[i] = orig;

                        // Some devices implement MMIO bar with 0 length, need to treat it as a no-bar
                        if (value == 0 && length == 0)
                            continue;
                        
                        // fix the length to support some "special" 64bit bar
                        if (value == 0)
                        {
                            value = 0xFFFFFFFF;
                        }
                        else
                        {
                            value |= 0xFFFFFFFF << BitOperations.Log2(value);
                        }
                        
                        // calculate the 64bit size and address
                        addr |= (ulong)orig << 32;
                        length |= (ulong)value << 32;
                        length = ~length + 1;
                        
                        break;
                    
                    default:
                        continue;
                }
            }

            if (mem)
            {
                if (length >= int.MaxValue)
                {
                    Debug.Print($"[PciDevice/{Bus:x02}:{Device:x02}.{Function:x}]: Bar at {idx} had a too big length {length} bytes, ignoring");
                }
                else
                {
                    MemoryBars[idx] = MemoryServices.Map(addr, (int)length);
                }

                var bits = mem64 ? "64" : "32";
                Debug.Print($"\tBars: [{idx}] Memory at {addr:x08} ({bits}-bit) [size={length}]");
            }
            else
            {
                Debug.Print($"\tBars: [{idx}] I/O ports at {addr:x04} [size={length}]");
            }
        }
    }

    private void SetCapabilities()
    {
        var list = new List<Memory<byte>>();

        if ((Header.Status & PciStatus.Capability) != 0)
        {
            var span = Config.Span;
            var ptr = span[PciSpec.CapabilityPointerOffset];
            while (ptr != 0)
            {
                var id = span[ptr + 0];
                var next = span[ptr + 1];
                var cap = Config.Slice(ptr);
                list.Add(cap);
                
                // Handle known caps in here
                switch (id)
                {
                    // TODO: MSI
                    case 0x11: Msix = new Msix(this, cap); break;
                }

                Debug.Print($"\tCapabilities: [{ptr:x02}] {PciSpec.PciCapabilityToStr(id)}");
                
                ptr = next;
            }
        }

        Capabilities = list.ToArray();
    }

}