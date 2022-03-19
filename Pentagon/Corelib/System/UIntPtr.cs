namespace System
{
    public readonly struct UIntPtr
    {
#pragma warning disable 169
        private readonly unsafe void* _value;
#pragma warning restore 169
    }
}