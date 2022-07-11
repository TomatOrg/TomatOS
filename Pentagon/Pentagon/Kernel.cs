using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;

namespace Pentagon;

public class Kernel
{
    public static int Main()
    {
        var acpi = new Acpi();
        var pci = new PciRoot(acpi);
        pci.AddDriver(new VirtioPciDriver());
        pci.Scan();
        return 0;
    }

}