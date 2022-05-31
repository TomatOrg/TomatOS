using System;

namespace Pentagon;

public class Kernel
{

    public class A
    {

        public virtual void Test()
        {
            
        }
        
    }

    public class B : A
    {

        public void Test()
        {
            
        }
        
    }

    public static int Main()
    {
        return 123;
    }
    
}