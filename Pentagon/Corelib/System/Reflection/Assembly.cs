namespace System.Reflection
{
    public class Assembly
    {

        private Module _module;
        private MethodInfo _entryPoint;
        
        private Type[] _definedTypes;
        private MethodInfo[] _definedMethods;
        private FieldInfo[] _definedFields;
        
        private Type[] _importedTypes;
        private MemberInfo[] _importedMembers;
        
        private byte[] _mirModule;
        private string[] _userStrings;
        private unsafe void* _userStringsTable;

    }
}