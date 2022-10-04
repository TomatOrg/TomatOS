using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;
using Pentagon.Drivers;
using Pentagon.Drivers.Graphics.Plain;
using Pentagon.DriverServices;
using Pentagon.DriverServices.Acpi;
using Pentagon.DriverServices.Pci;
using Pentagon.Managers;
using Pentagon.Graphics;
using Pentagon.Gui;
using Pentagon.Gui.Framework;
using Pentagon.Gui.Server;
using Pentagon.Gui.Widgets;
using Pentagon.Interfaces;
using Rectangle = Pentagon.Gui.Widgets.Rectangle;

public class Kernel
{
    internal static async Task Testing()
    {
        Log.LogString("hello\n");

        var root = await Fat32.stati.OpenVolume();
        Log.LogString("volume opened\n");

        var boot = await root.OpenDirectory("boot", 0);
        var newfile = await boot.CreateFile("helloworld", new DateTime(2001, 9, 11));
        var newdir = await boot.CreateDirectory("hellodir", new DateTime(2001, 9, 11));
        var lim = await boot.OpenFile("limine.cfg", 0);
        await boot.Rename(lim, "tomatboot.cfg");
        Log.LogString("completed\n");
    }
    
    private static Widget MainModel()
    {
        return new Stack(new Widget[]
        {
            // Background
            new Rectangle(0xff242424),
            
            // Contents
            new Padding(
                all: 32,
                child: new Column(new Widget[]
                    {
                        // A bit title
                        new Text("Hello World!", fontSize: 32),
                    }
                )
            )
        });
    }
    internal static async Task DoSomething() {
        unchecked
        {
            var root = await Fat32.stati.OpenVolume();
            var boot = await root.OpenDirectory("boot", 0);
            var fnt = await boot.OpenFile("ubuntu-regular.sdfnt", 0);
            int size = (int)fnt.FileSize;
            var data = new byte[size];
            var mem = new Memory<byte>(data);
            fnt.Read(0, mem, CancellationToken.None);
            Typeface.Load(mem);

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
        }
    }
    public static int Main()
    {
        // setup the basic subsystems
        var acpi = new Acpi();
        Pci.Scan(acpi);
        IoApic.Scan(acpi);
        PS2.Register(); // this is a misnomer, since it doesn't use ResourceManager yet, but we need AML for that
        Pentagon.Drivers.Virtio.VirtioDevice.Register();

        int dummy = 0;
        for (int i = 0; i < 1000000000; i++) Volatile.Read(ref dummy);
        
        DoSomething();

        return 0;
    }

}