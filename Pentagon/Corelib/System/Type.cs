using System.Reflection;

namespace System
{
    public abstract class Type : MemberInfo
    {

        private string _namespace;
        private FieldInfo[] _fields;
        // TODO: methods
        private Type _elementType;
        private Type _arrayType;

    }
}