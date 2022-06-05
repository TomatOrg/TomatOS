using System;
using System.Runtime.InteropServices;

namespace Pentagon;

public class Kernel
{

    public class Lol<T>
    {
        public T hello;

        public Lol(T hi)
        {
            hello = hi;
        }

    }
    
    public static int Main()
    {
        Lol<int> hi = new Lol<int>(123);
        Lol<string> hello = new Lol<string>("123");
        return hi.hello;
    }
    
}