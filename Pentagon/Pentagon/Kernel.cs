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
using Pentagon.Gui;
using Pentagon.Gui.Framework;
using Pentagon.Gui.Widgets;
using Pentagon.Interfaces;
using Pentagon.Resources;

namespace Pentagon;

public class Kernel
{


    private static Widget MainModel()
    {
        return new RectangleWidget(Color.Red);
    }

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
        
        // Create a plain graphics device (from a framebuffer) and 
        IGraphicsDevice dev = new PlainGraphicsDevice();
        var output = dev.Outputs[0];
        var framebuffer = dev.CreateFramebuffer(output.Width, output.Height);
        output.SetFramebuffer(framebuffer, new Rectangle(0, 0, output.Width, output.Height));

        // create the app and a local renderer to render the app
        var renderer = new LocalGuiServer(framebuffer);
        
        // run the app
        var app = new App(MainModel);
        app.Run(renderer);

        return 0;
    }

}