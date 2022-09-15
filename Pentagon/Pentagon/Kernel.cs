using System.Threading;
using Pentagon.Drivers;
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

        // start built-in drivers
        PS2.Register(); // this is a misnomer, since it doesn't use ResourceManager yet, but we need AML for that
        VirtioDevice.Register();

        return 0;
    }

}