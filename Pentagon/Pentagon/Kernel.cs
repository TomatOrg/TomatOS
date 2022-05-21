using System;

namespace Pentagon;

public class Kernel
{
    
    public static int Test(int a, int b)
    {
        return a + b;
    }

    public static int Main()
    {
        return Test(1, 2);
    }
    
}