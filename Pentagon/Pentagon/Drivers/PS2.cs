using System;
using Pentagon.DriverServices;
using Pentagon.DriverServices.Pci;
using Pentagon.Resources;
using System.Runtime.InteropServices;
using System.Buffers;
using System.Collections.Generic;
using System.Threading.Tasks;
using System.Threading;
using Pentagon.DriverServices;

namespace Pentagon.Drivers;

public class PS2
{
    const int PS2_IRQ = 1; // replace it with ACPI
    const ushort PS2_DATA_PORT = 0x60; // replace it with ACPI

    static Irq _kbdIrq;
    // TODO: use AML and have this actually register into the ResourceManager
    internal static void Register()
    {
        _kbdIrq = IoApic.RegisterIrq(PS2_IRQ);
        var irqThread = new Thread(IrqWait);
        irqThread.Start();
    }

    static char[] table = new char[] {
        '\0', '\0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
        'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', '\0', 'a', 's',
        'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', '\0', '\\', 'z', 'x', 'c', 'v',
        'b', 'n', 'm', ',', '.', '/', '\0', '\0', '\0', ' '
    };

    private static void IrqWait()
    {
        while (true)
        {
            _kbdIrq.Wait();
            // we need to read in order to acknowledge the interrupt
            var inputByte = IoPorts.In8(PS2_DATA_PORT);
            var arr = new char[1];
            if (inputByte < table.Length)
            {
                arr[0] = table[inputByte];
                var s = new string(arr);
                Log.LogString(s);
            }
        }
    }
}
