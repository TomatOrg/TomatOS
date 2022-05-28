using System;

namespace Pentagon;

public class Kernel
{

    public class A
    {
        public struct Lol
        {
            public int a;
            public int b;
        }
    
        public Lol lol;

        public int GetSum()
        {
            return lol.a + lol.b;
        }
    }
    
    public static int Main()
    {
        var a = new A();
        a.lol.a = 1;
        a.lol.b = 2;
        return a.GetSum();
    }
    
}