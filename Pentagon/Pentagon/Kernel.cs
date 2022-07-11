using System;
using System.Collections.Generic;
using Pentagon.HAL;

namespace Pentagon;

public class Kernel
{

    interface IA
    {
        int IA();
    }

    public class A : IA
    {
        public int IA()
        {
            return 123;
        }
    }
    
    public static int Main()
    {
        using var memory = MemoryServices.Map(10, 10);
        return memory.Memory.Length;
    }
    
}