using Pentagon.DriverServices;
using Pentagon.DriverServices.Pci;
using Pentagon.Resources;
using TinyDotNet.Reflection;

namespace Pentagon.Drivers.Virtio;

public class VirtioDevice
{

    private static bool CheckDevice(PciDevice device)
    {
        Log.LogHex(device.VendorId);
        
        // quickly filter devices which are not virtio
        if (device.VendorId != 0x1AF4)
            return false;
        
        // check for virtio-blk
        if (VirtioBlock.CheckDevice(device))
            return true;
        
        // TODO: check for other devices here
        
        return false;
    }
    
    /// <summary>
    /// Simply registers us for finding Virtio devices
    /// </summary>
    internal static void Register()
    {
        ResourceManager<PciDevice>.Register(CheckDevice);
    }
    
}