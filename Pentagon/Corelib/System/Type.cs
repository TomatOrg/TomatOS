using System.Reflection;
using System.Runtime.InteropServices;
using Pentagon;
using Pentagon.Reflection;

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
    private bool _isArray;
    private bool _isByRef;
    private Type[] _genericTypeArguments;
    private Type[] _genericTypeParameters;
    private Type[] _genericTypeDefinition;

    private int _classSize;
    private int _packingSize;

    private unsafe int* _managedPointersOffsets;
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
        
    private Type _arrayType;
    private Type _byRefType;
    private Mutex _typeMutex;
}