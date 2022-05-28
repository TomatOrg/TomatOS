using System;

namespace Pentagon;

public class Kernel
{

    public struct A
    {
        public int a;
        public int b;

        public int GetSum()
        {
            return a + b;
        }
    }
    
    public static int Main()
    {
        var arr = new A[10];

        for (int i = 0; i < arr.Length; i++)
        {
            arr[i].a = i;
            arr[i].b = i;
        }

        var sum = 0;
        for (int i = 0; i < arr.Length; i++)
        {
            sum += arr[i].GetSum();
        }

        return sum;
    }
    
}