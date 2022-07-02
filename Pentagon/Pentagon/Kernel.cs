using System;
using System.Threading;

namespace Pentagon;

public class Kernel
{

    public static int Main()
    {
        var test = new object();

        lock (test)
        {
            return 1;
        }
    }
    
}