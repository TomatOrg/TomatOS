using System.Runtime.InteropServices;

namespace System.Reflection;

[StructLayout(LayoutKind.Sequential)]
public class MethodInfo : MethodBase
{

    private Type _returnType;
    private bool _isFilled;
    private int _vtableOffset;
    private unsafe void* _mirFunc;
    private unsafe void* _mirProto;

}