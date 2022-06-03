using System;

namespace Pentagon;

public class Kernel
{

    public static object GetObject()
    {
        return 123;
    }
    
    public static int Main()
    {
        var a = GetObject();
        return (int)a;
    }
    
}