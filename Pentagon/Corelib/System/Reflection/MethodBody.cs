namespace System.Reflection
{
    public class MethodBody
    {

        private ExceptionHandlingClause[] _exceptionHandlingClauses;
        private LocalVariableInfo[] _localVariables;
        private bool _initLocals;
        private uint _maxStackSize;
        private byte[] _il;

    }
}