using System;
using Pentagon.DriverServices;
using Pentagon.DriverServices.Pci;
using Pentagon.Resources;
using System.Runtime.InteropServices;
using System.Buffers;
using System.Collections.Generic;
using System.Threading.Tasks;
using System.Threading;
using Pentagon.Interfaces;

namespace Pentagon.Drivers;

// TODO: put specifically keyboard related code here
internal class PS2Keyboard : IKeyboard
{
    Action<KeyEvent> _callback;
    Irq _irq;

    public void RegisterCallback(Action<KeyEvent> cb) => _callback = cb;

    static KeyCode[] set1 = new KeyCode[] {
        KeyCode.None,
        KeyCode.Escape,
        KeyCode.Num1,
        KeyCode.Num2,
        KeyCode.Num3,
        KeyCode.Num4,
        KeyCode.Num5,
        KeyCode.Num6,
        KeyCode.Num7,
        KeyCode.Num8,
        KeyCode.Num9,
        KeyCode.Num0,
        KeyCode.Hyphen,
        KeyCode.Equals,
        KeyCode.Backspace,
        KeyCode.Tab,
        KeyCode.Q,
        KeyCode.W,
        KeyCode.E,
        KeyCode.R,
        KeyCode.T,
        KeyCode.Y,
        KeyCode.U,
        KeyCode.I,
        KeyCode.O,
        KeyCode.P,
        KeyCode.LeftBrace,
        KeyCode.RightBrace,
        KeyCode.Enter,
        KeyCode.LeftCtrl,
        KeyCode.A,
        KeyCode.S,
        KeyCode.D,
        KeyCode.F,
        KeyCode.G,
        KeyCode.H,
        KeyCode.J,
        KeyCode.K,
        KeyCode.L,
        KeyCode.Punctuation3,
        KeyCode.Punctuation4,
        KeyCode.Punctuation5,
        KeyCode.LeftShift,
        KeyCode.Punctuation1,
        KeyCode.Z,
        KeyCode.X,
        KeyCode.C,
        KeyCode.V,
        KeyCode.B,
        KeyCode.N,
        KeyCode.M,
        KeyCode.Comma,
        KeyCode.Period,
        KeyCode.Slash,
        KeyCode.RightShift,
        KeyCode.NumpadMultiply,
        KeyCode.LeftAlt,
        KeyCode.Space,
        KeyCode.CapsLock,
        KeyCode.F1,
        KeyCode.F2,
        KeyCode.F3,
        KeyCode.F4,
        KeyCode.F5,
        KeyCode.F6,
        KeyCode.F7,
        KeyCode.F8,
        KeyCode.F9,
        KeyCode.F10,
        KeyCode.NumLock,
        KeyCode.ScrollLock,
        KeyCode.Numpad7,
        KeyCode.Numpad8,
        KeyCode.Numpad9,
        KeyCode.NumpadSubtract,
        KeyCode.Numpad4,
        KeyCode.Numpad5,
        KeyCode.Numpad6,
        KeyCode.NumpadAdd,
        KeyCode.Numpad1,
        KeyCode.Numpad2,
        KeyCode.Numpad3,
        KeyCode.Numpad0,
        KeyCode.NumpadPoint,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.F11,
        KeyCode.F12,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.MmPrevious,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.MmNext,
        KeyCode.None,
        KeyCode.None,
        KeyCode.NumpadEnter,
        KeyCode.RightCtrl,
        KeyCode.None,
        KeyCode.None,
        KeyCode.MmMute,
        KeyCode.MmCalc,
        KeyCode.MmPause,
        KeyCode.None,
        KeyCode.MmStop,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.MmQuieter,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.MmLouder,
        KeyCode.None,
        KeyCode.WwwHome,
        KeyCode.None,
        KeyCode.None,
        KeyCode.NumpadDivide,
        KeyCode.None,
        KeyCode.None,
        KeyCode.RightAlt,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.Home,
        KeyCode.UpArrow,
        KeyCode.PageUp,
        KeyCode.None,
        KeyCode.LeftArrow,
        KeyCode.None,
        KeyCode.RightArrow,
        KeyCode.None,
        KeyCode.End,
        KeyCode.DownArrow,
        KeyCode.PageDown,
        KeyCode.Insert,
        KeyCode.Delete,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.LeftFlag,
        KeyCode.RightFlag,
        KeyCode.ContextMenu,
        KeyCode.AcpiPower,
        KeyCode.AcpiSleep,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.AcpiWake,
        KeyCode.None,
        KeyCode.WwwSearch,
        KeyCode.WwwStarred,
        KeyCode.WwwRefresh,
        KeyCode.WwwStop,
        KeyCode.WwwForward,
        KeyCode.WwwBack,
        KeyCode.MmFiles,
        KeyCode.MmEmail,
        KeyCode.MmSelect,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None,
        KeyCode.None
    };

    internal PS2Keyboard()
    {
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
            var mask = (byte)(~0x80 & 0xFF);
            if (inputByte == 0xE0)
            {
                _irq.Wait();
                inputByte = PS2.KeyboardReceive();
                bool released = (inputByte & 0x80) != 0;
                var code = set1[(inputByte & mask) | 0x80];
                _callback(new KeyEvent(code, released));
            }
            else
            {
                bool released = (inputByte & 0x80) != 0;
                var code = set1[inputByte & mask];
                _callback(new KeyEvent(code, released));
            }
        }
    }
}


// TODO: put specifically keyboard related code here
internal class PS2Mouse
{
    Irq _irq;

    internal PS2Mouse()
    {
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

                    if ((statusPkt & 1) != 0)
                    {
                        Log.LogString("Left press\n");
                    }
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
