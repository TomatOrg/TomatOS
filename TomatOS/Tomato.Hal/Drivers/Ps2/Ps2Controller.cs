using System;
using System.IO;
using System.Threading;
using Tomato.Hal.Acpi.Resource;
using Tomato.Hal.Managers;

namespace Tomato.Hal.Drivers.Ps2;

[Flags]
public enum Ps2Status
{
    Obf = 1 << 0,
    Ibf = 1 << 1,
    CmdData = 1 << 3,
    MouseData = 1 << 5,
    TimeoutError = 1 << 6,
    ParityError = 1 << 7,
}

[Flags]
public enum Ps2Control : byte
{
    KeyboardIrq = 1 << 0,
    MouseIrq = 1 << 1,
    KeyboardDisable = 1 << 4,
    MouseDisable = 1 << 5,
    Translation = 1 << 6,
}

internal class Ps2Controller
{

    private const int MaxBufferSize = 16;
    
    private IoPort _command;
    private IoPort _data;

    internal Ps2Status Status => (Ps2Status)_command.ReadByte();

    internal byte Command
    {
        set => _command.WriteByte(value);
    }
    
    internal byte Data
    {
        get => _data.ReadByte();
        set => _data.WriteByte(value);
    }
    
    internal object Lock { get; } = new object();

    public Ps2Controller(IoResource command, IoResource data, IrqResource keyboardIrq, IrqResource mouseIrq)
    {
        _command = command[0];
        _data = data[0];
        
        // make sure there is nothing in the output
        // buffer of the device before doing anything
        // with it 
        Flush();
        
        // Perform a self-test on the device
        SelfTest();
        
        // now we are ready to init the controller
        Init();
        
        // now create the keyboard
        var keyboard = new Ps2Keyboard(this, keyboardIrq.Irqs[0]);
        DisplayManager.RegisterKeyboard(keyboard);
        
        // TODO: create the mouse 
    }

    private void WaitWrite()
    {
        while ((Status & Ps2Status.Ibf) != 0)
        {
        }
    }

    private void WaitRead()
    {
        while ((Status & Ps2Status.Obf) == 0)
        {
        }
    }

    internal byte ReadCommand(byte cmd)
    {
        // wait till we have an empty buffer
        WaitWrite();
        Command = cmd;

        WaitRead();
        return Data;
    }

    internal void WriteCommand(byte cmd, byte data)
    {
        WaitWrite();
        Command = cmd;

        WaitWrite();
        Data = data;
    }

    private void Init()
    {
        // read the config register
        var config = (Ps2Control)ReadCommand(0x20);
        
        // disable keyboard and mouse
        config |= Ps2Control.KeyboardDisable;
        config &= ~Ps2Control.KeyboardIrq;
        config |= Ps2Control.MouseDisable;
        config &= ~Ps2Control.MouseIrq;

        // write it back
        WriteCommand(0x60, (byte)config);
        
        // flush again just in case 
        Flush();
    }
    
    private void SelfTest()
    {
        // try a self test up to 5 times
        var i = 0;
        do
        {
            // check that we got back a success
            if (ReadCommand(0xAA) == 0x55)
                return;
            
            // failure, wait a bit before trying again
            Thread.Sleep(50);
        } while (i++ < 5);

        throw new IOException();
    }
    
    private void Flush()
    {
        while ((Status & Ps2Status.Obf) != 0)
        {
            _data.ReadByte();
        }
    }
    
}