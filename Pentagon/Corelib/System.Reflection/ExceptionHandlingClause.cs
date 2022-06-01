namespace System.Reflection
{
    public class ExceptionHandlingClause
    {
        private Type _catchType;
        private int _filterOffset;
        private ExceptionHandlingClauseOptions _flags;
        private int _handlerLength;
        private int _handlerOffset;
        private int _tryLength;
        private int _tryOffset;
    }
}