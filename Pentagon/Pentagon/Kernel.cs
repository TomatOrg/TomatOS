using System.Threading;
using Pentagon.Drivers;
using System;
using System.Drawing;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Pentagon.Drivers.Graphics.Plain;
using Pentagon.Drivers.Virtio;
using Pentagon.DriverServices;
using Pentagon.DriverServices.Acpi;
using Pentagon.DriverServices.Pci;
using Pentagon.Graphics;
using Pentagon.Interfaces;
using Pentagon.Resources;

namespace Pentagon;

public class Kernel
{

    public static int Main()
    {
        // setup the basic subsystems
        var acpi = new Acpi();
        // Pci.Scan(acpi);
        //
        // // register built-in drivers
        // VirtioDevice.Register();
        
        IoApic.Scan(acpi);
        PS2.Register(); // this is a misnomer, since it doesn't use ResourceManager yet, but we need AML for that

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // TODO: register the output 
        // get the device and output 
        //
        IGraphicsDevice dev = new PlainGraphicsDevice();
        var output = dev.Outputs[0];

        // allocate a framebuffer and set the backing for it 
        var framebuffer = dev.CreateFramebuffer(output.Width, output.Height);
        var backing = MemoryServices.AllocatePages(KernelUtils.DivideUp(output.Width * output.Height * 4, 4096)).Memory;
        framebuffer.Backing = backing;
        var memory = MemoryMarshal.Cast<byte, uint>(backing);
        
        // attach the framebuffer to the output
        output.SetFramebuffer(framebuffer, new Rectangle(0, 0, output.Width, output.Height));

        // update the framebuffer 
        
        {
            var blitter = new Blitter(memory, output.Width, (uint)Color.Red.ToArgb());
            blitter.BlitRect(10, 10, 100, 100);
        }
        
        {
            var blitter = new Blitter(memory, output.Width, (uint)Color.Green.ToArgb());
            blitter.BlitRect(50, 5, 20, 20);
        }
        
        {
            var blitter = new Blitter(memory, output.Width, (uint)Color.White.ToArgb());
            blitter.BlitRect(80, 20, 10, 10);
        }
        
        framebuffer.Blit(0, new Rectangle(0, 0, output.Width, output.Height));

        // draw to the screen
        framebuffer.Flush();
        
        return 0;
    }

}