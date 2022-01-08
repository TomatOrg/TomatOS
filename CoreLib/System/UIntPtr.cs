namespace System
{
    public readonly struct UIntPtr
    {
        
#pragma warning disable 169
        private readonly unsafe void* _value;
#pragma warning restore 169

        public unsafe UIntPtr(uint value)
        {
            _value = (void*) value;
        }

        public unsafe UIntPtr(ulong value)
        {
            _value = (void*) value;
        }

        public unsafe UIntPtr(void* value)
        {
            _value = value;
        }

        public unsafe uint ToUInt32()
        {
            return checked((uint) _value);
        }

        public unsafe ulong ToUInt64()
        {
            return (ulong) _value;
        }

    }
}