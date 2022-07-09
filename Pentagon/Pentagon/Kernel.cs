using System;

namespace Pentagon;

public class Kernel
{

    public static int Main()
    {
        using var pci = MemoryServices.MapPages(0x00000000B0000000, 256 * 64);
        return pci.Memory.Length;
    }
    
}