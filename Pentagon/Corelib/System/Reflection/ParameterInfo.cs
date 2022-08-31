using System.Runtime.InteropServices;

namespace System.Reflection;

[StructLayout(LayoutKind.Sequential)]
public class ParameterInfo
{

    private ushort _attributes;
    private string _name;
    private Type parameterType;

    internal ParameterInfo()
    {
    }

}