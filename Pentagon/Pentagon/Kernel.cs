using System;

namespace Pentagon;

public class Kernel
{

    public static object Lol(Nullable<int> a)
    {
        return a;
    }
    
    public static int Main()
    {
        return Lol(123) == null ? 1 : 0;
    }
    
}