using System;
using System.Collections;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace Pentagon.DriverServices.Pci;

/// <summary>
/// PCI function accessor. It offers manageable access to the config space
/// </summary>
public sealed class PciDevice
{

    /// Add PCI address of the device
    public readonly byte Bus;
    public readonly byte Device;
    public readonly byte Function;
    public readonly bool IsValid;

    // cached for ease of use 
    public readonly ushort VendorId;
    public readonly ushort DeviceId;
    
    /// <summary>
    /// The ECAM region of this device
    /// </summary>
    public readonly Region ConfigSpace;

    /// <summary>
    /// The config space of the device
    /// </summary>
    public ref PciConfigHeader ConfigHeader => ref _configHeader.Value;
    private readonly Field<PciConfigHeader> _configHeader;

    /// <summary>
    /// Points to the first capability 
    /// </summary>
    internal readonly Field<byte> CapabilitiesPointer;

    /// <summary>
    /// The amount of bars this device has 
    /// </summary>
    private readonly int _barCount = 0;

    /// <summary>
    /// MSI-X support on the pci device
    /// </summary>
    internal Msix Msix { get; private set; }

    public PciDevice(byte bus, byte dev, byte fn, Memory<byte> ecamSlice)
    {
        Bus = bus;
        Device = dev;
        Function = fn;
        ConfigSpace = new Region(ecamSlice);
        
        _configHeader = ConfigSpace.CreateField<PciConfigHeader>(0);
        
        IsValid = _configHeader.Value.VendorId != 0xFFFF;
        
        if (!IsValid)
            return;
        
        CapabilitiesPointer = ConfigSpace.CreateField<byte>(0x34);

        VendorId = ConfigHeader.VendorId;
        DeviceId = ConfigHeader.DeviceId;
        
        _barCount = (ConfigHeader.HeaderType & 0x7f) switch
        {
            0x00 => 6,
            0x01 => 2,
            _ => 0
        };
        
        InitBuiltinCaps();
    }

    /// <summary>
    /// Initialize any built-in caps that we support, this is mostly used for anything
    /// which we want to abstract in the kernel, so stuff like MSI/MSI-X, power management
    /// and so on.
    /// </summary>
    private void InitBuiltinCaps()
    {
        foreach (var cap in GetCapabilities())
        {
            switch (cap.Span[0])
            {
                // MSI-X
                case 0x11:
                {
                    Msix = new Msix(this, cap);
                } break;
            }
        }
    }

    #region Bar mapping

    public Memory<byte> MapBar(int index)
    {
        if (index >= _barCount)
            throw new ArgumentOutOfRangeException(nameof(index));

        // get the bar 
        var bar = ConfigSpace.AsSpan<uint>(0x10 + index * 4, 8);
        var barLow = bar[0];
        
        // make sure it is an io bar
        if ((barLow & 1) == 1) {
            //throw new NotImplementedException("IO Bars are currently not supported");
            return null;
        }

        // TODO: map prefetchable nicely
        
        // get the addr and size of the bar 
        var type = (barLow >> 1) & 0b11;
        ulong addr = 0;
        ulong size = 0;
        switch (type)
        {
            // 32bit bar 
            case 0:
                addr = barLow & 0xfffffff0;
                bar[0] = 0xffffffff;
                size = ~(bar[0] & 0xfffffff0) + 1;
                bar[0] = barLow;
                break;
            
            // 64bit bar
            case 2:
                var barHigh = bar[1];
                addr = barLow & 0xfffffff0 | ((ulong)barHigh << 32);
                bar[0] = 0xffffffff;
                bar[1] = 0xffffffff;
                size = ~(bar[0] & 0xfffffff0 | ((ulong)bar[1] << 32)) + 1;
                bar[0] = barLow;
                bar[1] = barHigh;
                break;
            
            default:
                throw new NotImplementedException();
        }
        
        // TODO: checked cast to int 
        return MemoryServices.Map(addr, (int)size);
    }

    #endregion
    
    #region Config space operations

    //
    // Utility functions to read from the config space
    // of pci devices, for optimized code its better to use
    // the Field<T> mechanism 
    // 

    public byte Read8(int offset)
    {
        var p = ConfigSpace.Span.Slice(offset);
        return p[0];
    }

    public ushort Read16(int offset)
    {
        var p = MemoryMarshal.Cast<byte, ushort>(ConfigSpace.Span.Slice(offset));
        return p[0];
    }

    public uint Read32(int offset)
    {
        var p = MemoryMarshal.Cast<byte, uint>(ConfigSpace.Span.Slice(offset));
        return p[0];
    }

    public void Write16(int offset, ushort value)
    {
        var p = MemoryMarshal.Cast<byte, ushort>(ConfigSpace.Span.Slice(offset));
        p[0] = value;
    }

    public void Write32(int offset, uint value)
    {
        var p = MemoryMarshal.Cast<byte, uint>(ConfigSpace.Span.Slice(offset));
        p[0] = value;
    }

    #endregion

    #region Capability enumeration

    /// <summary>
    /// Get an enumerator that allows to iterate all of the
    /// capabilities of the current device
    /// </summary>
    /// <returns></returns>
    public CapabilityEnumerator GetCapabilities()
    {
        return new CapabilityEnumerator(this);
    }
    
    /// <summary>
    /// Allows to iterate capabilities nicely
    /// </summary>
    public struct CapabilityEnumerator : IEnumerator<Memory<byte>>
    {
        object IEnumerator.Current => Current;
        public Memory<byte> Current { get; private set; }
        
        /// <summary>
        /// We keep this so we can access the device
        /// </summary>
        private readonly PciDevice _device;

        internal CapabilityEnumerator(PciDevice device)
        {
            _device = device;
            Current = Memory<byte>.Empty;
        }
        
        public bool MoveNext()
        {
            if (Current.IsEmpty)
            {
                // initialize from the base
                Current = _device.ConfigSpace.CreateMemory<byte>(_device.CapabilitiesPointer.Value);
            }
            else
            {
                // cap.Next of zero indicates the end of the capability list
                if (Current.Span[1] == 0)
                    return false;

                // yes, set it 
                Current = _device.ConfigSpace.CreateMemory<byte>(Current.Span[1]);
            }

            return true;
        }

        public void Reset()
        {
            Current = Memory<byte>.Empty;
        }

        public void Dispose()
        {
            // nothing to do...
        }

        public CapabilityEnumerator GetEnumerator()
        {
            return this;
        }
    }

    #endregion

}
