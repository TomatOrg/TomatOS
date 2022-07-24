using System;
using System.Threading;
public class A { }
public class Kernel
{
    static SemaphoreSlim sema;
    public static void Test1()
    {
        sema.Release();
    }

    public static int Main()
    {
        sema = new SemaphoreSlim(0);
        var t1 = new Thread(Test1);
        t1.Start();
        sema.Wait();

        return 0;
    }

}