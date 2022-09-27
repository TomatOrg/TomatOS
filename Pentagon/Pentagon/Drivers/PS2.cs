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

internal class PS2
{
    const int IRQ = 1; // replace it with ACPI
    const ushort DATA_PORT = 0x60; // replace it with ACPI
    
    const ushort STATUS_PORT = 0x64; // replace it with ACPI
    const byte STATUS_OUTBUFF_STATUS = 1 << 0;
    const byte STATUS_INBUFF_STATUS = 1 << 1; 

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
    const byte KBD_COMMAND_ENABLE = 0xF4;
    const byte KBD_COMMAND_SCANCODE_SET2 = 2;
    const byte KBD_COMMAND_RESET_DISABLE = 0xF5;

    private static void ControllerWaitRead()
    {
        while (true)
        {
            if ((IoPorts.In8(STATUS_PORT) & STATUS_OUTBUFF_STATUS) != 0) return;
            // TODO: delay
        }
    }

    private static void ControllerWaitWrite()
    {
        while (true)
        {
            if ((IoPorts.In8(STATUS_PORT) & STATUS_INBUFF_STATUS) == 0) return;
            // TODO: delay
        }   
    }
    
    private static void ControllerSendCommand(byte command)
    {
        ControllerWaitWrite();
        IoPorts.Out8(COMMAND_PORT, command);
    }

    private static void ControllerSendCommandParam(byte param)
    {
        ControllerWaitWrite();
        IoPorts.Out8(DATA_PORT, param);
    }

    private static byte ControllerReceiveCommandParam()
    {
        ControllerWaitRead();
        return IoPorts.In8(DATA_PORT);
    }

    private static void KeyboardSend(byte data)
    {
        ControllerWaitWrite();
        IoPorts.Out8(DATA_PORT, data);
    }
    private static byte KeyboardReceive()
    {
        // receive ack or error byte
        // TODO: actually ensure it succeeds
        ControllerWaitRead();
        return IoPorts.In8(DATA_PORT);
    }


    // TODO: use AML and have this actually register into the ResourceManager
    internal static void Register()
    {
        // ------------ initialize controller ------------
        // disable controllers during setup
        ControllerSendCommand(COMMAND_DISABLE_KBD);
        // this is acceptable, if the mouse isn't supported the command is a nop
        ControllerSendCommand(COMMAND_DISABLE_MOUSE);

        // flush output buffer
        while ((IoPorts.In8(STATUS_PORT) & STATUS_OUTBUFF_STATUS) != 0) IoPorts.In8(DATA_PORT);

        // TODO: do self test and keyboard test

        // get configuration
        ControllerSendCommand(COMMAND_READBYTE);
        var conf = ControllerReceiveCommandParam();

        // modify the configuration
        bool hasMouse = (conf & CONFIG_MOUSE_CLOCK) != 0;

        // disable irqs for now. TODO: make this less ugly
        conf &= (byte)(~((uint)(CONFIG_KBD_IRQ | CONFIG_MOUSE_IRQ)) & 0xFF);
        
        // i can just use set 1 and pretend the hell that is the late 90s PC industry never happened
        conf |= CONFIG_KBD_SET2_TO_1;

        // set configuration
        ControllerSendCommand(COMMAND_WRITEBYTE);
        ControllerSendCommandParam(conf);

        // setup finished, reenable controllers
        IoPorts.Out8(COMMAND_PORT, COMMAND_ENABLE_KBD);
        // here you need to check, enabling mouse when not supported is bad
        if (hasMouse) IoPorts.Out8(COMMAND_PORT, COMMAND_ENABLE_MOUSE);

        // ------------ initialize keyboard ------------
        KeyboardSend(KBD_COMMAND_RESET_DISABLE);
        KeyboardReceive();

        // yes, here we need to put set 2 if we want to receive set 1. KbdWriteCommands sends commands to the keyboard, not the controller
        // and CONFIG_KBD_SET2_TO_1 specifies that it wants to take set 2 from the keyboard to convert it to set 1 scancodes for the kernel
        // if for some cursed reason the keyboard sends set 3 or set 1, the translation will still interpret them as set 2 
        KeyboardSend(KBD_COMMAND_SCANCODE);
        KeyboardSend(KBD_COMMAND_SCANCODE_SET2);
        KeyboardReceive();

        KeyboardSend(KBD_COMMAND_ENABLE);
        KeyboardReceive();

        // ------------ terminate controller setup ------------
        // enable interrupts
        ControllerSendCommand(COMMAND_READBYTE);
        conf = ControllerReceiveCommandParam();
        conf |= CONFIG_KBD_IRQ;
        if (hasMouse) conf |= CONFIG_MOUSE_IRQ;
        ControllerSendCommand(COMMAND_WRITEBYTE);
        ControllerSendCommandParam(conf);

        // do i need to flush again?

        _kbdIrq = IoApic.RegisterIrq(IRQ);
        var irqThread = new Thread(IrqWait);
        irqThread.Start();
    }

    // roslyn does not recognize \e


    const byte SCANCODE_CTRL = 0x1D; // right ctrl is E0,1D
    const byte SCANCODE_RSHIFT = 0x36;
    const byte SCANCODE_LSHIFT = 0x2A;
    const byte SCANCODE_CAPSLOCK = 0x3A;

    const char ESC = (char)0x1B;

    static char[] asciiCapslock = new char[] {
        '\0', ESC, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', 
        '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', '\n', '\0',
        'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', '\'', '`', '\0', '\\',
        'Z', 'X', 'C', 'V', 'B', 'N', 'M', ',', '.', '/', '\0', '\0', '\0', ' '
    };

    static char[] asciiShift = new char[] {
        '\0', ESC, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
        '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', '\0',
        'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', '\0', '|',
        'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', '\0', '\0', '\0', ' '
    };

    static char[] asciiShiftCapslock = new char[] {
        '\0', ESC, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
        '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '{', '}', '\n', '\0',
        'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ':', '"', '~', '\0', '|',
        'z', 'x', 'c', 'v', 'b', 'n', 'm', '<', '>', '?', '\0', '\0', '\0', ' '
    };

    static char[] asciiNoMod = new char[] {
        '\0', ESC, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
        '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', '\0',
        'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', '\0', '\\',
        'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', '\0', '\0', '\0', ' '
    };

    private static void IrqWait()
    {
        // remember that right shift and ctrl exist
        int shift = 0, ctrl = 0, capslock = 0;

        while (true)
        {
            _kbdIrq.Wait();
            // we need to read in order to acknowledge the interrupt
            var inputByte = IoPorts.In8(DATA_PORT);
            if (inputByte == 0xE0)
            {
                _kbdIrq.Wait();
                inputByte = IoPorts.In8(DATA_PORT);
                switch (inputByte)
                {
                    case SCANCODE_CTRL:
                        ctrl++;
                        break;
                    case SCANCODE_CTRL | 0x80:
                        ctrl--;
                        break;
                    default:
                        break;
                }
            }
            else
            {
                switch (inputByte)
                {
                    case SCANCODE_CTRL:
                        ctrl++;
                        break;
                    case SCANCODE_CTRL | 0x80:
                        ctrl--;
                        break;
                    case SCANCODE_LSHIFT:
                        shift++;
                        break;
                    case SCANCODE_LSHIFT | 0x80:
                        shift--;
                        break;
                    case SCANCODE_RSHIFT:
                        shift++;
                        break;
                    case SCANCODE_RSHIFT | 0x80:
                        shift--;
                        break;
                    case SCANCODE_CAPSLOCK:
                        capslock++;
                        break;
                    case SCANCODE_CAPSLOCK | 0x80:
                        capslock--;
                        break;
                    default:
                        break;
                }
                char[] table;
                if (shift > 0  &&  capslock > 0) table = asciiShiftCapslock;
                else if (shift > 0  && capslock == 0) table = asciiShift;
                else if (shift == 0 && capslock > 0) table = asciiCapslock;
                else table = asciiNoMod;

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
}
