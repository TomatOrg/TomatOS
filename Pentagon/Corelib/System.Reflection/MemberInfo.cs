using System.Runtime.InteropServices;

namespace System.Reflection;

[StructLayout(LayoutKind.Sequential)]
public class MemberInfo
{

    private Type _declaringType;
    private Module _module;
    private string _name;

}