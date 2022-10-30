using System;
using System.Runtime.InteropServices;
using System.Buffers;
using System.Collections.Generic;
using System.Threading.Tasks;
using System.Threading;
using Tomato.Hal.Interfaces;
using Tomato.Hal.Platform.Pc;
using Tomato.Hal;

namespace Tomato.Hal;

// TODO: put specifically keyboard related code here
internal class PS2Keyboard : IKeyboard
{
    KeyboardCallback _callback = null;
    Irq _irq;

    public void RegisterCallback(KeyboardCallback cb)
    {
        _callback = cb;
        _irq = IoApic.RegisterIrq(PS2.KBD_IRQ);
        var irqThread = new Thread(IrqWait);
        irqThread.Start();
    }

    private void IrqWait()
    {
        while (true)
        {
            _irq.Wait();
            var inputByte = PS2.KeyboardReceive();
            if (_callback is null) continue;

            var mask = (byte)(~0x80 & 0xFF);
            if (inputByte == 0xE0)
            {
                _irq.Wait();
                inputByte = PS2.KeyboardReceive();
                bool released = (inputByte & 0x80) != 0;
                var code = (inputByte & mask) | 0x80;
                _callback(new KeyEvent(code, released));
            }
            else
            {
                bool released = (inputByte & 0x80) != 0;
                var code = inputByte & mask;
                _callback(new KeyEvent(code, released));
            }
        }
    }
}


// TODO: put specifically mouse related code here
internal class PS2Mouse : IRelMouse
{
    Irq _irq;
    RelMouseCallback _callback = null;

    public void RegisterCallback(RelMouseCallback cb)
    {
        _callback = cb;
        _irq = IoApic.RegisterIrq(PS2.MOUSE_IRQ);
        var irqThread = new Thread(IrqWait);
        irqThread.Start();
    }

    private void IrqWait()
    {
        int cycle = 0;
        short xPkt = 0, yPkt = 0, statusPkt = 0;
        while (true)
        {
            _irq.Wait();
            var data = PS2.MouseReceive();
            if (_callback is null) continue;

            switch (cycle)
            {
                case 0:
                    statusPkt = data;
                    cycle++;
                    break;
                case 1:
                    xPkt = data;
                    cycle++;
                    break;
                case 2:
                    yPkt = data;
                    cycle = 0;
                    // x and y are 9bit twos complemebt numnber
                    if ((statusPkt & (1 << 4)) != 0) xPkt = (short)((ushort)xPkt | 0xFF00);
                    if ((statusPkt & (1 << 5)) != 0) yPkt = (short)((ushort)yPkt | 0xFF00);

                    bool left = ((statusPkt & 1) != 0);
                    bool right = ((statusPkt & 2) != 0);
                    _callback(new RelMouseEvent(xPkt, -yPkt, left, right));

                    break;
            }
        }
    }
}

internal class PS2
{
    internal static PS2Keyboard Keyboard;
    internal static PS2Mouse Mouse;

    internal const int KBD_IRQ = 1; // replace it with ACPI
    internal const int MOUSE_IRQ = 12; // replace it with ACPI
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
    const byte COMMAND_SEND_TO_MOUSE = 0xD4;


    const byte CONFIG_KBD_IRQ = 1 << 0;
    const byte CONFIG_MOUSE_IRQ = 1 << 1;
    const byte CONFIG_PASSED_POST = 1 << 2;
    const byte CONFIG_KBD_CLOCK = 1 << 4;
    const byte CONFIG_MOUSE_CLOCK = 1 << 5;
    const byte CONFIG_KBD_SET2_TO_1 = 1 << 6;

    const byte KBD_COMMAND_SCANCODE = 0xF0;
    const byte KBD_COMMAND_ENABLE = 0xF4;
    const byte KBD_COMMAND_SCANCODE_SET2 = 2;
    const byte KBD_COMMAND_RESET_DISABLE = 0xF5;

    const byte MOUSE_COMMAND_SELFTEST = 0xFF;
    const byte MOUSE_COMMAND_SETDEFAULTS = 0xF6;
    const byte MOUSE_COMMAND_ENABLE_DATA_REPORTING = 0xF4;

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

    private static void ControllerFlush()
    {
        while ((IoPorts.In8(STATUS_PORT) & STATUS_OUTBUFF_STATUS) != 0) IoPorts.In8(DATA_PORT);
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

    internal static byte KeyboardReceive()
    {
        // receive ack or error byte
        // TODO: actually ensure it succeeds
        ControllerWaitRead();
        return IoPorts.In8(DATA_PORT);
    }
    private static void MouseSend(byte data)
    {
        ControllerWaitWrite();
        IoPorts.Out8(COMMAND_PORT, COMMAND_SEND_TO_MOUSE);
        ControllerWaitWrite();
        IoPorts.Out8(DATA_PORT, data);
    }
    internal static byte MouseReceive()
    {
        // TODO: assert that im reading from mouse
        ControllerWaitRead();
        return IoPorts.In8(DATA_PORT);
    }


    // TODO: use AML and have this actually register into the ResourceManager
    internal static void Register()
    {
        // ------------ initialize controller ------------
        ControllerSendCommand(COMMAND_DISABLE_KBD);
        ControllerSendCommand(COMMAND_DISABLE_MOUSE);
        ControllerFlush();

        // get configuration
        ControllerSendCommand(COMMAND_READBYTE);
        var conf = ControllerReceiveCommandParam();

        conf |= CONFIG_KBD_IRQ;
        conf |= CONFIG_MOUSE_IRQ;
        conf |= CONFIG_KBD_SET2_TO_1; // i can just use set 1 and pretend the hell that is the late 90s PC industry never happened

        // set configuration
        ControllerSendCommand(COMMAND_WRITEBYTE);
        ControllerSendCommandParam(conf);

        // setup finished, reenable controllers
        ControllerSendCommand(COMMAND_ENABLE_KBD);
        ControllerSendCommand(COMMAND_ENABLE_MOUSE);

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

        // ------------ initialize mouse ------------
        MouseSend(MOUSE_COMMAND_SELFTEST);
        MouseReceive();
        MouseSend(MOUSE_COMMAND_SETDEFAULTS);
        MouseReceive();
        MouseSend(MOUSE_COMMAND_ENABLE_DATA_REPORTING);
        MouseReceive();

        // ------------ terminate setup ------------
        ControllerFlush();
        Keyboard = new PS2Keyboard();
        Mouse = new PS2Mouse();
    }
}
