using System.Runtime.InteropServices;

namespace System.Reflection;

[StructLayout(LayoutKind.Sequential)]
public class MethodBase : MemberInfo
{

    private ushort _implAttributes;
    private ushort _attributes;
    private MethodBody _methodBody;
    private ParameterInfo[] _parameters;

}