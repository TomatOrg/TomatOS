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
        var lol = new A.Lol();
        lol.a = 1;
        lol.b = 2;
        a.lol = lol;
        return a.GetSum();
    }
    
}