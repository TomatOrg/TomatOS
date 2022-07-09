using System;

namespace Pentagon;

public class Kernel
{

    public struct Lol
    {
        public int a;
        public int b;
    }

    public static void A(Lol[] a, Lol b)
    {
        a[0] = b;
    }
    
    public static int Main()
    {
        var lol = new Lol();
        lol.a = 123;
        lol.b = 456;
        var lolArr = new Lol[10];
        A(lolArr, lol);
        return lolArr[0].a + lolArr[0].b;
    }
    
}