using System;
using System.Diagnostics;
using System.IO;
using System.Threading;
using Tomato.Hal.Interfaces;

namespace Tomato.Hal.Drivers.Ps2;

internal class Ps2Keyboard : IKeyboard
{
    
    public KeyboardCallback Callback { get; set; }

    private readonly Ps2Controller _controller;
    private readonly Irq _irq;

    private bool _extended = false;

    internal Ps2Keyboard(Ps2Controller controller, Irq irq)
    {
        _controller = controller;
        _irq = irq;

        // setup the interrupt handler
        var thread = new Thread(IrqThread)
        {
            Name = GetType().FullName
        };
        thread.Start();
        
        // and now enable the keyboard
        Enable();
    }

    private void Enable()
    {
        lock (_controller.Lock)
        {
            // read the config register
            var config = (Ps2Control)_controller.ReadCommand(0x20);
        
            // enable keyboard and interrupt 
            config &= ~Ps2Control.KeyboardDisable;
            config |= Ps2Control.KeyboardIrq;
        
            // write it back
            _controller.WriteCommand(0x60, (byte)config);
        }
    }

    private void HandleIrq()
    {
        byte data;
        
        lock (_controller.Lock)
        {
            // check the status to see if we got any data
            var status = _controller.Status;
            if ((status & Ps2Status.Obf) == 0)
            {
                return;
            }

            // get the data from the device
            data = _controller.Data;
            
            // check for anything weird
            if ((status & Ps2Status.ParityError) != 0)
            {
                throw new IOException();
            }
            
            if ((status & Ps2Status.TimeoutError) != 0)
            {
                throw new TimeoutException();
            }
        }
        
        var released = (data & 0x80) != 0;
        var code = (data & 0x7F);

        // handle the extended input
        if (_extended)
        {
            // the last byte was 0xE0, so we need to set
            // the new data properly
            _extended = false;
            
            // set the hight bit for this 
            code |= 0x80;
            
        } else if (data == 0xE0)
        {
            // we need to wait for another packet
            // with more data
            _extended = true;
            
            return;
        }
        
        // send it to the listener 
        Callback?.Invoke(new KeyEvent(code, released));
    }
    
    private void IrqThread()
    {
        while (true)
        {
            _irq.Wait();

            try
            {
                HandleIrq();
            }
            catch (Exception e) when (e is IOException or TimeoutException)
            {
                Debug.WriteLine($"[{GetType().FullName}] {e}");
            }
        }
    }

}