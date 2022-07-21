using Pentagon.DriverServices;
using Pentagon.DriverServices.Pci;

namespace Pentagon.Drivers.Virtio;

public class VirtioBlock
{
    
    internal static bool CheckDevice(PciDevice device)
    {
        if (device.DeviceId != 0x1001 && device.DeviceId != 0x1042)
            return false;

        // TODO: create the device
        
        Log.LogHex(0xCAFE);
        
        return false;
    }
    
}