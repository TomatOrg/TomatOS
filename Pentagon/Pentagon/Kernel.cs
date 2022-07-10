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
        var memory = new Memory<byte>(new byte[10]);
        var region = new Region(memory);
        var int0 = region.CreateField<int>(0);
        var int1 = region.CreateField<int>(4);
        var long0 = region.CreateField<long>(0);
        int0.Value = 123;
        int1.Value = 456;
        return (int)long0.Value;
    }
    
}