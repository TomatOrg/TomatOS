using System.Runtime.InteropServices;

namespace System.Reflection;

[StructLayout(LayoutKind.Sequential)]
public class MemberInfo
{

    internal Type _declaringType;
    internal Module _module;
    internal string _name;

    public string Name => _name;

}