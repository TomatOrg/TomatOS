using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Threading;

namespace Pentagon;

public class Kernel
{

    public static int DoIt(Func<int, int, int> op)
    {
        return op(1, 2);
    }
    
    public static int Main()
    {
        return DoIt((x, y) => x + y);
    }
    
}