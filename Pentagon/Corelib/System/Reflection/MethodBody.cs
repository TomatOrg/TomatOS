using System.Runtime.InteropServices;

namespace System.Reflection;

[StructLayout(LayoutKind.Sequential)]
public class MethodBody
{

    private ExceptionHandlingClause[] _exceptionHandlingClauses;
    private LocalVariableInfo[] _localVariables;
    private bool _initLocals;
    private uint _maxStackSize;
    private byte[] _il;

    internal MethodBody()
    {
    }
    
}