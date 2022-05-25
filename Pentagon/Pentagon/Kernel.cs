using System;

namespace Pentagon;

public class Kernel
{

    public interface INumber
    {
        public int GetNumber();

    }

    public interface INumberLong
    {
        public long GetLong();

    }
    
    class A : INumber, INumberLong
    {
        public int GetNumber()
        {
            return 1;
        }
        
        public long GetLong()
        {
            return 123;
        }
    }

    class B : INumber
    {

        public int GetNumber()
        {
            return 2;
        }
    }

    public static int Main()
    {
        INumber num = new B();
        return num.GetNumber();
    }
    
}