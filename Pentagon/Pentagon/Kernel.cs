using Pentagon.DriverServices;

using System;
using System.Threading;

namespace Pentagon;

public class Kernel
{

    [ThreadStatic] public static int MyThing = 0;

    public static void FromMyThread()
    {
        Log.LogHex((ulong)MyThing);
        MyThing = 0xDEAD;
        Log.LogHex((ulong)MyThing);
    }
    
    public static int Main()
    {
        Log.LogHex((ulong)MyThing);
        MyThing = 0xBABE;
        Log.LogHex((ulong)MyThing);

        new Thread(FromMyThread).Start();

        // // setup the basic subsystems
        // var acpi = new Acpi();
        // Pci.Scan(acpi);
        //
        // // register built-in drivers
        // VirtioDevice.Register();
        return 0;
    }

}