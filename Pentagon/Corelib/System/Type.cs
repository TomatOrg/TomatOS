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
        private Type _arrayType;
        private ulong _stackSize;
        private ulong _managedSize;
    }
}