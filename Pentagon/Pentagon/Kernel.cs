using System.Drawing;
using Pentagon.Drivers.Graphics;
using Pentagon.Drivers.Graphics.Raster;
using Pentagon.Drivers.Virtio;
using Pentagon.DriverServices;
using Pentagon.DriverServices.Acpi;
using Pentagon.DriverServices.Pci;

namespace Pentagon;

public class Kernel
{
        
    public static int Main()
    {
        // setup the basic subsystems
        // var acpi = new Acpi();
        // Pci.Scan(acpi);
        //
        // // register built-in drivers
        // VirtioDevice.Register();

        var index = 0;
        if (!KernelUtils.GetNextFramebuffer(ref index, out var addr, out var width, out var height, out var pitch))
        {
            Log.LogString("sadge");
            return -1;
        }
        var buffer = MemoryServices.Map(addr, pitch * height);

        var surface = new RasterSurface(width, height, buffer, pitch);
        var canvas = new Canvas(surface.Canvas);

        canvas.Clear(Color.Red);
        
        return 0;
    }

}