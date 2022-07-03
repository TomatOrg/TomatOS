using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using TinyDotNet.Reflection;

namespace System;

[StructLayout(LayoutKind.Sequential)]
public abstract class Type : MemberInfo
{

    private Assembly _assembly;
    private Type _baseType;
    private string _namespace;
    private FieldInfo[] _fields;
    private MethodInfo[] _methods;
    private Type _elementType;
    private uint _attributes;
    private int _metadataToken;
    private bool _isArray;
    private bool _isByRef;
    private bool _isBoxed;

    private Type _genericTypeDefinition;
    private int _genericTypeAttributes;
    private int _genericParmaeterPosition;
    private Type[] _genericArguments;

    private MethodInfo _delegateSignature;

    private int _classSize;
    private int _packingSize;

    private unsafe int* _managedPointersOffsets;
    private bool _isSetup;
    private bool _isFilled;
    private bool _isValueType;
    private MethodInfo[] _virtualMethods;
    private MethodInfo _finalize;
    private int _managedSize;
    private int _managedAlignment;
    private int _stackSize;
    private int _stackAlignment;
    private unsafe void* _vtable;
    private int _stackType;
    private MethodInfo _staticCtor;
        
    private InterfaceImpl[] _interfaceImpl;
    private MethodImpl[] _methodImpls;
    private unsafe void* _mirType;
        
    private Type _arrayType;
    private Type _byRefType;
    private Type _boxedType;
    private Type _unboxedType;
    private Type _nextGenericInstance;

    private Type _nextNestedType;
    private Type _nestedTypes;

    internal int StackSize => _stackSize;

    [MethodImpl(MethodImplOptions.InternalCall)]
    public static extern Type GetTypeFromHandle(RuntimeTypeHandle handle);

}