namespace System.Reflection
{
    public class Assembly
    {

        private Type[] _definedTypes;
        private MethodInfo[] _definedMethods;
        private FieldInfo[] _definedFields;
        private Module _module;
        private string[] _userStrings;
        private unsafe void* _userStringsTable;

    }
}