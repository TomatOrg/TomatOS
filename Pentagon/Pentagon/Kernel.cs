using System;

namespace Pentagon;

public class Kernel
{

    class A
    {
        public virtual int GetNumber()
        {
            return 1;
        }
    }

    class B : A
    {
        public override int GetNumber()
        {
            return 5;
        }        
    }
    
    public static int Test(int a, int b)
    {
        return a + b;
    }

    public static int Main()
    {
        A a = new B();
        return a.GetNumber();
    }
    
}