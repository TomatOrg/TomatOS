using System.Runtime.InteropServices;
using TinyDotNet.Reflection;

namespace System.Reflection;

[StructLayout(LayoutKind.Sequential)]
public class Assembly
{

    private string _name;
    private ushort _majorVersion;
    private ushort _minorVersion;
    private ushort _buildNumber;
    private ushort _revisionNumber;
        
    private Module _module;
    private MethodInfo _entryPoint;
        
    private Type[] _definedTypes;
    private MethodInfo[] _definedMethods;
    private FieldInfo[] _definedFields;
    private byte[][] _definedTypeSpecs;
    private MemberReference[] _definedMemberRefs;
    private MethodSpec[] _definedMemberSpecs;
        
    private Type[] _importedTypes;
        
    private string[] _userStrings;
    private unsafe void* _userStringsTable;

}