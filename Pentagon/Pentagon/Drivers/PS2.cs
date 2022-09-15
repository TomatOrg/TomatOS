using System;
using Pentagon.DriverServices;
using Pentagon.DriverServices.Pci;
using Pentagon.Resources;
using System.Runtime.InteropServices;
using System.Buffers;
using System.Collections.Generic;
using System.Threading.Tasks;
using System.Threading;

namespace Pentagon.Drivers;

public class PS2
{
    const int PS2_IRQ = 1; // replace it with ACPI

    static Irq _kbdIrq;
    // TODO: use AML and have this actually register into the ResourceManager
    internal static void Register()
    {
        _kbdIrq = IoApic.RegisterIrq(PS2_IRQ);
        var irqThread = new Thread(IrqWait);
        irqThread.Start();
    }
    private static void IrqWait()
    {
        while (true)
        {
            _kbdIrq.Wait();
            // we need to read in order to acknowledge the interrupt
            Log.LogString("!");
        }
    }
}
