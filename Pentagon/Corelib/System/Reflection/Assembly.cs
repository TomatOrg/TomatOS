using System.Runtime.CompilerServices;
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
    private PropertyInfo[] _definedProperties;
    private ParameterInfo[] _definedParameters;
    private byte[][] _definedTypeSpecs;
    private MemberReference[] _definedMemberRefs;
    private MethodSpec[] _definedMemberSpecs;
        
    private Type[] _importedTypes;
        
    private unsafe void* _userStringsTable;
    private unsafe void* _customAttributeMap;

    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    private extern static Assembly LoadInternal(byte[] rawAssembly, bool reflection); 
    
    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    private extern static Assembly LoadInternal(string rawAssembly, bool reflection);
    
    internal Assembly()
    {
    }
    
    [MethodImpl(MethodCodeType = MethodCodeType.Native)]
    extern ~Assembly();
    
    public static Assembly Load(byte[] rawAssembly)
    {
        if (rawAssembly == null)
            throw new ArgumentNullException(nameof(rawAssembly));

        var asm = LoadInternal(rawAssembly, false);
        if (asm == null)
            throw new BadImageFormatException();
        
        return asm;
    }

    public static Assembly Load(string assemblyString)
    {
        if (assemblyString == null)
            throw new ArgumentNullException(nameof(assemblyString));

        var asm = LoadInternal(assemblyString, false);
        if (asm == null)
            throw new BadImageFormatException();
        
        return asm;
    }

    public static Assembly ReflectionOnlyLoad(byte[] rawAssembly)
    {
        if (rawAssembly == null)
            throw new ArgumentNullException(nameof(rawAssembly));

        var asm = LoadInternal(rawAssembly, true);
        if (asm == null)
            throw new BadImageFormatException();
        
        return asm;
    }

    public static Assembly ReflectionOnlyLoad(string assemblyString)
    {
        if (assemblyString == null)
            throw new ArgumentNullException(nameof(assemblyString));

        var asm = LoadInternal(assemblyString, true);
        if (asm == null)
            throw new BadImageFormatException();
        
        return asm;
    }
    
}