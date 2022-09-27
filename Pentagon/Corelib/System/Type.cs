using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using TinyDotNet.Reflection;

namespace System;

[StructLayout(LayoutKind.Sequential)]
public class Type : MemberInfo
{

    private Assembly _assembly;
    private Type _baseType;
    private string _namespace;
    private FieldInfo[] _fields;
    private MethodInfo[] _methods;
    private PropertyInfo[] _properties;
    private Type _elementType;
    private uint _attributes;
    private int _metadataToken;
    private bool _isArray;
    private bool _isByRef;
    private bool _isBoxed;
    private bool _isPointer;

    private Type _genericTypeDefinition;
    private int _genericTypeAttributes;
    private int _genericParmaeterPosition;
    private Type[] _genericArguments;

    private MethodInfo _delegateSignature;

    private int _classSize;
    private int _packingSize;

    private unsafe int* _managedPointersOffsets;
    private bool _isSetup;
    private bool _isSetupFinished;
    private bool _isFilled;
    private bool _isValueType;
    private MethodInfo _finalize;
    private int _managedSize;
    private int _managedAlignment;
    private int _stackSize;
    private int _stackAlignment;
    private int _stackType;
    private MethodInfo _staticCtor;

    private MethodInfo[] _virtualMethods;
    private unsafe void* _vtable;
    private int _vtableSize;

    private InterfaceImpl[] _interfaceImpl;
    private MethodImpl[] _methodImpls;
    private unsafe void* _mirType;
        
    private Type _arrayType;
    private Type _byRefType;
    private Type _boxedType;
    private Type _pointerType;
    private Type _unboxedType;
    private Type _nextGenericInstance;

    private Type _nextNestedType;
    private Type _nestedTypes;

    internal Type()
    {
    }

    public bool IsArray => _isArray;
    public bool IsByRef => _isByRef;

    public bool IsEnum => _baseType == typeof(Enum);

    public Type GetElementType()
    {
        return _elementType;
    }
    
    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    public static extern Type GetTypeFromHandle(RuntimeTypeHandle handle);
    
    public string FullName
    {
        get
        {
            if (_declaringType != null)
            {
                return string.Concat(_declaringType.FullName, "+", _name);
            }

            return !string.IsNullOrEmpty(_namespace) ? string.Concat(_namespace, ".", _name) : _name;
        }
    }

    public bool IsPointer => _isPointer;

    public bool IsGenericTypeDefinition => _genericArguments != null && _genericTypeDefinition == null;
    
    public bool ContainsGenericParameters
    {
        get
        {
            if (_genericParmaeterPosition >= 0)
                return true;

            if (_isArray)
                return _elementType.ContainsGenericParameters;

            if (_isByRef)
                return _baseType.ContainsGenericParameters;

            if (_genericArguments != null)
            {
                foreach (var arg in _genericArguments)
                {
                    if (arg.ContainsGenericParameters)
                        return true;
                }
            }

            return false;
        }
    }
    
    public bool IsValueType => _isValueType;
    
    public bool IsSubclassOf(Type c)
    {
        var current = this;
        while (current != typeof(object))
        {
            if (current == c)
            {
                return true;
            }
            current = current._baseType;
        }

        return false;
    }

    public override string ToString()
    {
        return FullName;
    }
}