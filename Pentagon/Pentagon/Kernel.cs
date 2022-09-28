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
    static Memory<byte> kbdLayout;
    static int shift = 0, altgr = 0;
    static void KbdCallback(KeyEvent k)
    {
        if (!k.Released && (k.Code == KeyCode.LeftShift || k.Code == KeyCode.RightShift)) { shift = 1; return; }
        if (k.Released && (k.Code == KeyCode.LeftShift || k.Code == KeyCode.RightShift)) { shift = 0; return; }

        if (!k.Released && (k.Code == KeyCode.RightAlt)) { altgr = 1; return; }
        if (k.Released && (k.Code == KeyCode.RightAlt)) { altgr = 0; return; }

        if (k.Released) return;
        
        var c = GetCodepoint(k.Code, shift > 0, altgr > 0);
        char[] chars = new char[1];
        chars[0] = (char)c;
        Log.LogString(new string(chars));

    }

    private static Widget MainModel()
    {
        return new RectangleWidget(Color.Red);
    }

    // TODO: use Rune
    internal static int GetCodepoint(KeyCode c, bool shiftHeld, bool altGrHeld)
    {
        int off = (int)c;
        //if (off >= 0x200) throw exception
        if (shiftHeld) off |= 0x200;
        if (altGrHeld) off |= 0x400;
        var offset = MemoryMarshal.Cast<byte, ushort>(kbdLayout).Span[off];
        // TODO: proper unicode
        int codepoint = kbdLayout.Span[0x1000 + offset];
        if ((codepoint & 0xE0) == 0xC0) // twobyte UTF8
        {
            codepoint = (codepoint & 0x1F) << 6;
            codepoint |= kbdLayout.Span[0x1001 + offset] & 0x3F;
        }
        return codepoint;
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
        PS2.Keyboard.RegisterCallback(KbdCallback);
        
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