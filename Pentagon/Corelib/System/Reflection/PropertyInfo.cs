using System.Runtime.InteropServices;

namespace System.Reflection;

[StructLayout(LayoutKind.Sequential)]
public class PropertyInfo : MemberInfo
{

    private ushort _attributes;
    private MethodInfo _setMethod;
    private MethodInfo _getMethod;
    private Type _propertyType;

    internal PropertyInfo()
    {
    }
    
}