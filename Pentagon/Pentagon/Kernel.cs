using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;

namespace Pentagon;

public class Kernel
{

    public static ManualResetEvent Event = new(false);

    public static void Lol()
    {
        // sleep a little
        Thread.Sleep(1000);
        
        // then set it 
        Event.Set();
        
        // sleep a little
        Thread.Sleep(1000);
        
        // then set it 
        Event.Set();
    }
    
    public static int Main()
    {
        var thread = new Thread(Lol);
        thread.Start();

        // wait for it 
        Event.WaitOne();
        Event.WaitOne();
        Event.WaitOne();
        Event.WaitOne();

        Event.Reset();
        
        // again 
        Event.WaitOne();

        return 1;
    }
}