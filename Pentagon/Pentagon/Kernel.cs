using System.Threading;
using Pentagon.Drivers;
using System;
using System.Diagnostics.CodeAnalysis;
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
using Pentagon.Gui.Server;
using Pentagon.Gui.Widgets;
using Pentagon.Interfaces;
using Pentagon.Resources;
using Rectangle = Pentagon.Gui.Widgets.Rectangle;

namespace Pentagon;

public class Kernel
{
    static internal Memory<byte> kbdLayout;

    // TODO: use Rune
    internal static int GetCodepoint(KeyCode c, bool shiftHeld, bool altGrHeld)
    {
        int off = (int)c;
        //if (off >= 0x200) throw exception
        if (shiftHeld) off |= 0x200;
        if (altGrHeld) off |= 0x400;
        var offset = MemoryMarshal.Cast<byte, ushort>(Kernel.kbdLayout).Span[off];
        // TODO: proper unicode
        int codepoint = Kernel.kbdLayout.Span[0x1000 + offset];
        if ((codepoint & 0xE0) == 0xC0) // twobyte UTF8
        {
            codepoint = (codepoint & 0x1F) << 6;
            codepoint |= Kernel.kbdLayout.Span[0x1001 + offset] & 0x3F;
        }
        return codepoint;
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
                child: new Rectangle(Color.Red)
            )
        });
    }

    public static int Main()
    {
        KernelUtils.GetKbdLayout(out ulong kbdAddr, out ulong kbdSize);
        kbdLayout = MemoryServices.Map(kbdAddr, (int)kbdSize);

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
        output.SetFramebuffer(framebuffer, new System.Drawing.Rectangle(0, 0, output.Width, output.Height));

        // create the app and a local renderer to render the app
        var renderer = new LocalGuiServer(framebuffer, PS2.Keyboard);
        
        // run the app
        var app = new App(MainModel);
        app.Run(renderer);

        return 0;
    }

}