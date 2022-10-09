using System.Numerics;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Runtime.Serialization;
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
    private short _packingSize;

    private unsafe int* _managedPointersOffsets;
    private bool _isValueType;
    private MethodInfo _finalize;
    private int _managedSize;
    private int _managedAlignment;
    private int _stackSize;
    private int _stackAlignment;
    private int _stackType;
    private MethodInfo _typeInitializer;

    private uint _fillingFlags;
    
    private MethodInfo[] _virtualMethods;
    private unsafe void* _vtable;

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

    public Assembly Assembly => _assembly;

    public bool IsArray => _isArray;
    public bool IsByRef => _isByRef;

    public bool IsInterface => (_attributes & 0x00000020) != 0;
    
    public bool IsEnum => _baseType == typeof(Enum);

    public Type GetElementType()
    {
        return _elementType;
    }

    public Type[] GetGenericArguments()
    {
        return _genericArguments.AsSpan().ToArray();
    }

    public bool IsConstructedGenericType
    {
        get
        {
            // TODO: does this return true for non-generic types
            if (_genericArguments == null)
                return false;

            // just return is this contains any generic parameters
            return !ContainsGenericParameters;
        }
    }

    public Type GetGenericTypeDefinition()
    {
        return _genericTypeDefinition;
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

    public static TypeCode GetTypeCode(Type type)
    {
        return type.GetTypeCode();
    }

    public bool IsGenericType => _genericArguments != null;

    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Native)]
    private extern Type InternalMakeGenericType(Type[] typeArguments);

    public Type MakeGenericType(params Type[] typeArguments)
    {
        if (!IsGenericTypeDefinition)
            throw new InvalidOperationException(this.ToString());
        
        if (typeArguments == null)
            throw new ArgumentNullException();
        
        foreach (var arg in typeArguments)
        {
            if (arg == null)
                throw new ArgumentNullException();

            if (arg.IsPointer || arg == typeof(void) || arg.IsByRef)
                throw new ArgumentException();
        }

        if (typeArguments.Length != _genericArguments.Length)
            throw new ArgumentException();
        
        // TODO: bubble constraints checking from the internal function

        return InternalMakeGenericType(typeArguments);
    }

    public bool IsAssignableFrom(Type c)
    {
        if (c == null)
            return false;
        
        // Represents the same type or derived either directly
        // or indirectly from the current instance.
        var type = this;
        while (type != null)
        {
            if (type == c)
                return true;

            type = type._baseType;
        }

        // The current instance is an interface that c implements
        if (IsInterface && c._interfaceImpl != null)
        {
            foreach (var impl in c._interfaceImpl)
            {
                if (impl._interfaceType == this)
                {
                    return true;
                }
            }
        }

        // c represents a value type and the current instance represents Nullable<c>
        if (c.IsValueType && _genericTypeDefinition == typeof(Nullable<>) && _genericArguments[0] == c)
        {
            return true;
        }

        return false;
    }

    public bool IsAssignableTo(Type other)
    {
        return other.IsAssignableFrom(this);
    }
    
    public override string ToString()
    {
        return FullName;
    }
}