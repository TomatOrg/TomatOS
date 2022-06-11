using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace Pentagon;

public class Kernel
{
    
    public static int Main()
    {
        int a = 123;
        int b = a;
        return b.Equals(a) ? 1 : 0;
    }
    
}