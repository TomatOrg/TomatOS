using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;

namespace Pentagon;

public class Kernel
{
    public static int Main()
    {
        var task = MyOtherAsyncMethod();
        YieldAwaitable.StupidContinuation(); // continue and yield back, this is a test
        return task.Result;
    }
    private static async Task<int> MyOtherAsyncMethod()
    {
        await Task.Yield();
        return 6;
    }
}