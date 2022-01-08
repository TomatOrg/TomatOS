namespace System
{
    public readonly struct Int16
    {
        
#pragma warning disable 169
        private readonly short _value;
#pragma warning restore 169

        public const short MaxValue = (short)0x7FFF;
        public const short MinValue = unchecked((short)0x8000);
        
    }
}