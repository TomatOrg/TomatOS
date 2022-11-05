using System;
using System.Threading.Tasks;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Runtime.InteropServices;
using Tomato.Hal.Interfaces;
using Tomato.Hal;
using Tomato.Hal.Io;
using Tomato.Hal.Managers;
using Tomato.Hal.Pci;

namespace Tomato.Drivers.Fat;

internal static class Program
{
    public static void Main()
    {
        var instance = new FatDriver();
        BlockManager.RegisterDriver(instance);
    }
}