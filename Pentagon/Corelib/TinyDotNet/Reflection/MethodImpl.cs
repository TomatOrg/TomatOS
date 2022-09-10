using System.Reflection;
using System.Runtime.InteropServices;

namespace TinyDotNet.Reflection;

[StructLayout(LayoutKind.Sequential)]
public class MethodImpl
{
    
    private MethodInfo _body;
    private MethodInfo _declaration;

    private MethodImpl()
    {
    }
    
}