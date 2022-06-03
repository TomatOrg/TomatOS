using System.Runtime.InteropServices;

namespace System.Reflection;

[StructLayout(LayoutKind.Sequential)]
public class LocalVariableInfo
{
    private int _localIndex;
    private Type _localType;
}