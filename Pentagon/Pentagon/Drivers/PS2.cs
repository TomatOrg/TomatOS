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
    const int IRQ = 1; // replace it with ACPI
    const ushort DATA_PORT = 0x60; // replace it with ACPI
    
    const ushort STATUS_PORT = 0x64; // replace it with ACPI
    const byte STATUS_OUTBUFF_FULL = 1 << 0;
    const byte STATUS_INBUFF_FULL = 1 << 1;

    const ushort COMMAND_PORT = 0x64; // replace it with ACPI
    const byte COMMAND_READBYTE = 0x20;
    const byte COMMAND_WRITEBYTE = 0x60;
    const byte COMMAND_DISABLE_KBD = 0xAD;
    const byte COMMAND_ENABLE_KBD = 0xAE;
    const byte COMMAND_DISABLE_MOUSE = 0xA7;
    const byte COMMAND_ENABLE_MOUSE = 0xA8;


    const byte CONFIG_KBD_IRQ = 1 << 0;
    const byte CONFIG_MOUSE_IRQ = 1 << 1;
    const byte CONFIG_PASSED_POST = 1 << 2;
    const byte CONFIG_KBD_CLOCK = 1 << 4;
    const byte CONFIG_MOUSE_CLOCK = 1 << 5;
    const byte CONFIG_KBD_SET2_TO_1 = 1 << 6;

    static Irq _kbdIrq;

    const byte KBD_COMMAND_SCANCODE = 0xF0;
    const byte KBD_COMMAND_SCANCODE_SET2 = 2;

    // TODO: use AML and have this actually register into the ResourceManager
    internal static void Register()
    {
        // disable controllers during setup
        IoPorts.Out8(COMMAND_PORT, COMMAND_DISABLE_KBD);
        // this is acceptable, if the mouse isn't supported the command is a nop
        IoPorts.Out8(COMMAND_PORT, COMMAND_DISABLE_MOUSE);

        // flush output buffer
        while ((IoPorts.In8(STATUS_PORT) & STATUS_OUTBUFF_FULL) != 0) IoPorts.In8(DATA_PORT);

        // TODO: osdev wiki says to do self tests, but I'll skip those

        // get configuration
        IoPorts.Out8(COMMAND_PORT, COMMAND_READBYTE);
        var conf = IoPorts.In8(DATA_PORT);

        // modify the configuration
        bool hasMouse = (conf & CONFIG_MOUSE_CLOCK) != 0;
        // enable keyboard irq and mouse if supported
        conf |= CONFIG_KBD_IRQ;
        if (hasMouse) conf |= CONFIG_MOUSE_IRQ;
        // i can just use set 1 and pretend the hell that is the late 90s PC industry never happened
        conf |= CONFIG_KBD_SET2_TO_1;

        // set configuration
        IoPorts.Out8(COMMAND_PORT, COMMAND_WRITEBYTE);
        IoPorts.Out8(DATA_PORT, conf);

        // setup finished, reenable controllers
        IoPorts.Out8(COMMAND_PORT, COMMAND_ENABLE_KBD);
        // here you need to check, enabling mouse when not supported is bad
        if (hasMouse) IoPorts.Out8(COMMAND_PORT, COMMAND_ENABLE_MOUSE);

        // TODO:
        // yes, here we need to put set 2 if we want to receive set 1. KbdWriteCommands sends commands to the keyboard, not the controller
        // and CONFIG_KBD_SET2_TO_1 specifies that it wants to take set 2 from the keyboard to convert it to set 1 scancodes for the kernel
        // if for some cursed reason the keyboard sends set 3 or set 1, the translation will still interpret them as set 2 
        // KbdWrite(KBD_COMMAND_SCANCODE);
        // KbdWrite(KBD_COMMAND_SCANCODE_SET2);

        // do i need to flush again? i think so
        
        _kbdIrq = IoApic.RegisterIrq(IRQ);
        var irqThread = new Thread(IrqWait);
        irqThread.Start();
    }

    static char[] _set1Base = new char[] {
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
            var inputByte = IoPorts.In8(DATA_PORT);
            if (inputByte == 0xE0)
            {
                _kbdIrq.Wait();
                IoPorts.In8(DATA_PORT);
            }
            else
            {
                var arr = new char[1];
                if (inputByte < _set1Base.Length)
                {
                    arr[0] = _set1Base[inputByte];
                    var s = new string(arr);
                    Log.LogString(s);
                }
            }
        }
    }
}
