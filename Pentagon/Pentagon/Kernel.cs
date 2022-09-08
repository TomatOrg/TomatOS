using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;
using Pentagon.Drivers;
using Pentagon.Drivers.Virtio;
using Pentagon.DriverServices;
using Pentagon.DriverServices.Acpi;
using Pentagon.DriverServices.Pci;
using Pentagon.Managers;

public class Kernel
{
    internal static async Task Testing()
    {
        Log.LogString("hello\n");

        var root = await Fat32.stati.OpenVolume();
        Log.LogString("volume opened\n");

        var boot = await root.OpenDirectory("boot", 0);
        var newfile = await boot.CreateFile("helloworld", new DateTime(2001, 9, 11));
        var newdir = await boot.CreateDirectory("hellodir", new DateTime(2001, 9, 11));
        var lim = await boot.OpenFile("limine.cfg", 0);
        await boot.Rename(lim, "tomatboot.cfg");
        Log.LogString("completed\n");
    }
    public static int Main()
    {
        // setup the basic subsystems
        var acpi = new Acpi();
        Pci.Scan(acpi);
        
        // register built-in drivers
        VirtioDevice.Register();

        int dummy = 0;
        for (int i = 0; i < 100000000; i++) Volatile.Read(ref dummy);
        Testing();
        
        return 0;
    }

}