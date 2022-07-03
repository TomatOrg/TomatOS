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
        var arr = new int[3];
        arr[0] = 1;
        arr[1] = 2;
        arr[2] = 3;

        var span = new Span<int>(arr);
        arr[0] = 123;
        return span[0];
    }
}