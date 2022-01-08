namespace System
{
    public readonly struct Char
    {
        
#pragma warning disable 169
        private readonly char _value;
#pragma warning restore 169
        
        public const char MaxValue = (char)0xFFFF;
        public const char MinValue = (char)0x00;
        
    }
}