using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace Pentagon;

public class Kernel
{
    
    public static int Main()
    {
        List<int> a = new List<int>();
        a.Add(1);
        a.Add(2);
        a.Add(3);

        var sum = 0;
        foreach (var item in a)
        {
            sum += item;
        }
        
        return sum;
    }
    
}