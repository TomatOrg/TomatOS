using System.Reflection;
using Pentagon;

namespace System
{
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
        private bool _isPointer;
        private Type[] _genericTypeArguments;
        private Type[] _genericTypeParameters;
        private Type[] _genericTypeDefinition;

        private unsafe int* _managedPointersOffsets;
        private bool _isFilled;
        private bool _isValueType;
        private MethodInfo[] VirtualMethods;
        private int _managedSize;
        private int _managedAlignment;
        private int _stackSize;
        private int _stackAlignment;
        
        private Type _arrayType;
        private Mutex _arrayTypeMutex;
    }
}