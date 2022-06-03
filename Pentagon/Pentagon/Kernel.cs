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
        object a = GetObject();
        return 456;
    }
    
}