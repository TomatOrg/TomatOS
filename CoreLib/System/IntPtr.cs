namespace System
{
    public readonly struct IntPtr
    {
        
#pragma warning disable 169
        private readonly unsafe void* _value;
#pragma warning restore 169

        public unsafe IntPtr(int value)
        {
            _value = (void*) value;
        }

        public unsafe IntPtr(long value)
        {
            _value = (void*) value;
        }

        public unsafe int ToInt32()
        {
            var l = (long) _value;
            return checked((int) l);
        }

        public unsafe long ToInt64()
        {
            return (long) _value;
        }
        
    }
}