using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;

namespace Pentagon;

public class Kernel
{

    interface A
    {
        
    }

    interface B : A
    {
        
    }

    interface C : B
    {
        
    }
    
    public static int Main()
    {
        var framebufferOwner = MemoryServices.MapPages(0x00000000FD000000, 1);
        framebufferOwner.Dispose();
        return Unsafe.SizeOf<int>();
    }
    
}