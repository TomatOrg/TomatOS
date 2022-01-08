namespace System
{
    public readonly struct Int32
    {

#pragma warning disable 169
        private readonly int _value;
#pragma warning restore 169
        
        public const int MaxValue = 0x7fffffff;
        public const int MinValue = unchecked((int) 0x80000000);
        
    }
}