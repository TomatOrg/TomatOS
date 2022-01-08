namespace System
{
    public readonly struct UInt64
    {
        
#pragma warning disable 169
        private readonly ulong _value;
#pragma warning restore 169

        public const ulong MaxValue = (ulong)0xffffffffffffffffL;
        public const ulong MinValue = 0x0;
        
    }
}