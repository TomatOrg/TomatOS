namespace System
{
    public class Object
    {
        private Type _type;
        
        //
        // Internal to the runtime, do not touch
        //
        private UIntPtr _log_pointer;
        private byte _color;
        private byte _rank;
        private byte _reserved0;
        private byte _reserved1;
        private UIntPtr _next;
        private UIntPtr _chunkNext;

    }
}