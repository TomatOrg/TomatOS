using System;

namespace Pentagon;

public class Kernel
{

    public class A
    {

        public A a;
        
        ~A()
        {
            throw new Exception("Hello world!");
        }
        
    }
    
    public static int Main()
    {
        var a = new A();
        var b = new A();
        a.a = b;
        return 1;
    }
    
}