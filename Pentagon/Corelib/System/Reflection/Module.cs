using System.Runtime.InteropServices;

namespace System.Reflection;

[StructLayout(LayoutKind.Sequential)]
public class Module
{

    private Assembly _assembly;
    private string _name;

    internal Module()
    {
    }

}