using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.InteropServices;

namespace Tomato.Hal.Pci;

public static class PciManager
{

    private static Dictionary<uint, Type> _drivers = new();
    private static Dictionary<uint, Type> _fallbackDrivers = new();
    private static List<PciDevice> _devices = new();
    private static object _lock = new();

    private static bool _locked = false;
    
    private static uint GetDriverHash(ushort vendorId, ushort deviceId)
    {
        return ((uint)vendorId << 16) |  deviceId;
    }

    private static uint GetFallbackHash(byte classCode, byte subclass, byte progIf)
    {
        return ((uint)classCode << 16) | ((uint)subclass << 8) | progIf;
    }

    private static bool DispatchDevice(PciDevice device)
    {
        if (!_drivers.TryGetValue(GetDriverHash(device.VendorId, device.DeviceId), out var driver))
        {
            if (!_fallbackDrivers.TryGetValue(GetFallbackHash(device.ClassCode, device.SubclassCode, device.ProgIf), out driver))
            {
                // not found 
                return false;
            }
        }

        // try to setup the driver
        // TODO: handle exceptions
        Activator.CreateInstance(driver, device);
        
        // we found a driver
        return true;
    }

    private static void DispatchAllDevices()
    {
        for (var i = 0; i < _devices.Count; i++)
        {
            if (!DispatchDevice(_devices[i])) 
                continue;
            
            // found driver, remove the device from the list
            _devices.RemoveAt(i);
            i--;
        }
    }
    
    internal static void RegisterDevice(PciDevice device)
    {
        lock (_lock)
        {
            if (!DispatchDevice(device))
            {
                _devices.Add(device);
            }
        }
    }

    /// <summary>
    /// Locks the PciManager from accepting new drivers
    /// </summary>
    public static void Lock()
    {
        lock (_lock)
        {
            _locked = true;
            
            // we can clear the drivers left out since they are def not loaded
            _devices.Clear();
        }
    }

    /// <summary>
    /// Register a new PCI driver, must be called before the Lock method is called
    /// or an exception will be thrown
    /// </summary>
    public static void RegisterDriver(Type type)
    {
        // fast path
        if (_locked)
            throw new InvalidOperationException();

        lock (_lock)
        {
            if (_locked)
                throw new InvalidOperationException();

            var dispatch = false;

            // get all the drivers supported by the class
            foreach (var attr in Attribute.GetCustomAttributes(type, typeof(PciDriverAttribute)))
            {
                var driver = (PciDriverAttribute)attr;
                if (driver.VendorId != 0xFFFF)
                {
                    // normal driver
                    var hash = GetDriverHash(driver.VendorId, driver.DeviceId);
                    if (_drivers.TryAdd(hash, type))
                    {
                        // try to dispatch all left devices in case this driver
                        // can handle it 
                        dispatch = true;
                    }
                }
                else
                {
                    // fallback driver
                    var hash = GetFallbackHash(driver.ClassCode, driver.Subclass, driver.ProgIf);
                    if (_fallbackDrivers.TryAdd(hash, type))
                    {
                        // try to dispatch all left devices in case this driver
                        // can handle it 
                        dispatch = true;
                    }
                }
            }

            // check if any of the new drivers can handle an existing device
            if (dispatch)
            {
                DispatchAllDevices();
            }
        }
    }

    #region Scanning
    
    // TODO: this will eventually move to be a PCI driver, then we will properly support
    //       stuff like multiple segments/domain and all the other fun stuff
    
    internal struct PciScan
    {
        
        private Memory<byte> _ecam;
        
        public PciScan(Memory<byte> ecam)
        {
            _ecam = ecam;
        }

        private void ScanDevice(int bus, int dev, int func = 0)
        {
            // get the config
            var config = _ecam.Slice(((bus) << 20) | (dev << 15) | (func << 12), 4096);
            ref var header = ref MemoryMarshal.Cast<byte, PciHeader>(config).Span[0];

            // vendor is invalid, ignore
            if (header.VendorId == 0xFFFF)
                return;

            // create the device
            RegisterDevice(new PciDevice(config, bus, dev, func));
            
            // multifunction device discovery
            if (func == 0 && header.IsPciMultiFunc)
            {
                for (var i = 1; i <= PciSpec.MaxFunc; i++)
                {
                    ScanDevice(bus, dev, i);
                }
            }
            
            // finally discover secondary bridge
            if (!header.IsPciBridge)
                return;

            // get the secondary
            ScanBus(config.Span[PciSpec.BirdgeSecondaryBusRegisterOffset]);
        }

        private void ScanBus(int bus)
        {
            for (var dev = 0; dev <= PciSpec.MaxDevice; dev++)
            {
                ScanDevice(bus, dev);
            }
        }
        
        public void Scan()
        {
            // start scanning from the start
            ScanBus(0);
        }
        
    }

    #endregion

}