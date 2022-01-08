namespace System
{
    public readonly struct UInt32
    {

#pragma warning disable 169
        private readonly uint _value;
#pragma warning restore 169
        
        public const uint MaxValue = (uint)0xffffffff;
        public const uint MinValue = 0U;
        
    }
}