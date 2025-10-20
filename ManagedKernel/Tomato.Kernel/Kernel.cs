using System.Diagnostics;

namespace Tomato.Kernel;

internal static class Kernel
{

    private static int Main()
    {
        Debug.WriteLine("Hello from managed kernel!");
        return 0;
    }
    
}