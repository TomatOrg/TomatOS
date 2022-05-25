using System;

namespace Pentagon;

public class Kernel
{

    public abstract class Number
    {
        public abstract int GetNumber();

    }

    class A : Number
    {
        public override int GetNumber()
        {
            return 1;
        }
    }

    class B : Number
    {

        public override int GetNumber()
        {
            return 2;
        }
    }

    public static int Main()
    {
        Number num = new B();
        return num.GetNumber();
    }
    
}