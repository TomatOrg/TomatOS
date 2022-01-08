namespace System
{
    public readonly struct Byte
    {

#pragma warning disable 169
        private readonly byte _value;
#pragma warning restore 169
        
        public const byte MaxValue = (byte)0xFF;
        public const byte MinValue = 0;

    }
}