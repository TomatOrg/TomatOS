using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Runtime.InteropServices;
using Tomato.Hal.Interfaces;
using Tomato.Hal.Managers;
using Tomato.Hal.Pci;

namespace Tomato.Drivers.Virtio;

internal static class Program
{
    public static void Main()
    {
        PciManager.RegisterDriver(typeof(VirtioBlock));
    }
}