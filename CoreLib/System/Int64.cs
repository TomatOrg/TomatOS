namespace System
{
    public readonly struct Int64
    {
        
#pragma warning disable 169
        private readonly long _value;
#pragma warning restore 169

        public const long MaxValue = 0x7fffffffffffffffL;
        public const long MinValue = unchecked((long)0x8000000000000000L);
        
    }
}