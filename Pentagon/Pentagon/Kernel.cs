using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using System.Threading;
using Pentagon.Drivers;
using Pentagon.Drivers.Virtio;
using Pentagon.DriverServices;
using Pentagon.DriverServices.Acpi;
using Pentagon.DriverServices.Pci;

public class Kernel
{

    public static int Main()
    {
        // setup the basic subsystems
        var acpi = new Acpi();
        Pci.Scan(acpi);
        
        // register built-in drivers
        VirtioDevice.Register();

        int dummy = 0;
        for (long i = 0; i < 1000000000; i++) Volatile.Read(ref dummy);
        Fat32.testing.PrintRoot();
        return 0;
    }

}