using System;

namespace Pentagon;

public class Kernel
{

    public interface IA
    {
        public int GetNumber();
    }
    
    public class A : IA
    {

        public int GetNumber()
        {
            return 123;
        }
    }

    public static IA GetIA()
    {
        return new A();
    }
    
    public static object GetObject()
    {
        return GetIA();
    }
    
    public static int Main()
    {
        var a = GetObject();
        return ((IA)a).GetNumber();
    }
    
}