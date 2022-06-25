using System.Threading;

namespace Pentagon;

public class Kernel
{

    public static int Main()
    {
        var e = new AutoResetEvent(true);
        
        Thread.Sleep(1000);
        
        return e.WaitOne(1000) ? 1 : 0;
    }
    
}