namespace System.Reflection
{
    public class FieldInfo : MemberInfo
    {
        private Type _fieldType;
        private nuint _memoryOffset;
        private ushort _attributes;

    }
}