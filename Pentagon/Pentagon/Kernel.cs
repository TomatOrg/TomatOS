using Pentagon.Drivers.Virtio;
using Pentagon.DriverServices;
using Pentagon.DriverServices.Acpi;
using Pentagon.DriverServices.Pci;

namespace Pentagon;

public class Kernel
{
    
    public static int Main()
    {
        // setup the basic subsystems
        var acpi = new Acpi();
        Pci.Scan(acpi);
        IoApic.Scan(acpi);

        // register built-in drivers
        VirtioDevice.Register();
        return 0;
    }

}