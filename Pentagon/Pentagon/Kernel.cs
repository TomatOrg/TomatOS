using Pentagon.Drivers;
using Pentagon.Drivers.Graphics.Plain;
using Pentagon.DriverServices;
using Pentagon.DriverServices.Acpi;
using Pentagon.Graphics;
using Pentagon.Gui;
using Pentagon.Gui.Framework;
using Pentagon.Gui.Server;
using Pentagon.Gui.Widgets;
using Pentagon.Interfaces;
using Rectangle = Pentagon.Gui.Widgets.Rectangle;

namespace Pentagon;

public class Kernel
{


    private static Widget MainModel()
    {
        return new Stack(new Widget[]
        {
            // Background
            new Rectangle(0xff242424),
            
            // Contents
            new Padding(
                all: 32,
                child: new Text("Hello World!", fontSize: 64)
            )
        });
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
        
        // load the default font 
        Typeface.Load();

        // Create a plain graphics device (from a framebuffer) and 
        IGraphicsDevice dev = new PlainGraphicsDevice();
        var output = dev.Outputs[0];
        var framebuffer = dev.CreateFramebuffer(output.Width, output.Height);
        output.SetFramebuffer(framebuffer, new System.Drawing.Rectangle(0, 0, output.Width, output.Height));

        // create the app and a local renderer to render the app
        var renderer = new LocalGuiServer(framebuffer);
        
        // run the app
        var app = new App(MainModel);
        app.Run(renderer);

        return 0;
    }

}