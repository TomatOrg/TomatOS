using System.Runtime.InteropServices;

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
        
    private Type[] _importedTypes;
    private MemberInfo[] _importedMembers;
        
    private string[] _userStrings;
    private unsafe void* _userStringsTable;

}