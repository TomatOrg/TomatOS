using System.Reflection;

namespace System
{
    public abstract class Type : MemberInfo
    {

        private Assembly _assembly;
        private Type _baseType;
        private string _namespace;
        private FieldInfo[] _fields;
        private Type _elementType;
        private uint _attributes;
        private bool _isArray;
        private bool _isByRef;
        private bool _isPointer;
        private unsafe void* _arrayTypeMutex; 
        private Type _arrayType;
        private unsafe nuint* _managedPointersOffsets;
        private nuint _stackSize;
        private nuint _stackAlignment;
        private nuint _managedSize;
        private nuint _managedAlignment;
        private bool _sizeValid;
        private bool _isValueType;

    }
}