namespace System
{
    public readonly struct SByte
    {

#pragma warning disable 169
        private readonly sbyte _value;
#pragma warning restore 169

        public const sbyte MaxValue = (sbyte) 0x7F;
        public const sbyte MinValue = unchecked((sbyte) 0x80);

    }
}