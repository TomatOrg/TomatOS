using System.Runtime.InteropServices;

namespace System.Reflection;

[StructLayout(LayoutKind.Sequential)]
public class FieldInfo : MemberInfo
{
    private Type _fieldType;
    private nuint _memoryOffset;
    private ushort _attributes;

    internal FieldInfo()
    {
    }

}