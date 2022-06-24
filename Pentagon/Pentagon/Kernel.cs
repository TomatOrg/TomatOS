using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;

namespace Pentagon;

public class Kernel
{

    public static int Main()
    {
        var watch = Stopwatch.StartNew();
        
        var sum = 0;
        for (var i = 0; i < 100; i++)
        {
            sum += i;
        }
        
        watch.Stop();
        return (int)watch.ElapsedMilliseconds;
    }
    
}