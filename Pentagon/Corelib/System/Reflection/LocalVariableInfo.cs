using System.Runtime.InteropServices;

namespace System.Reflection;

[StructLayout(LayoutKind.Sequential)]
public class LocalVariableInfo
{

    private bool _isPinned;
    private int _localIndex;
    private Type _localType;

    public bool IsPinned => _isPinned;
    public int LocalIndex => _localIndex;
    public Type LocalType => _localType;

    internal LocalVariableInfo()
    {
    }
    
}