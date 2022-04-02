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

        public Type GetType()
        {
            return _type;
        }
        
        public virtual bool Equals(object obj)
        {
            return ReferenceEquals(this, obj);
        }
        
        public static bool Equals(object objA, object objB)
        {
            if (objA == objB)
            {
                return true;
            }
            
            if (objA == null || objB == null)
            {
                return false;
            }
            
            return objA.Equals(objB);
        }

        public virtual int GetHashCode()
        {
            return 0;
        }

        public static bool ReferenceEquals(object objA, object objB)
        {
            return objA == objB;
        }

        public virtual string ToString()
        {
            return GetType().ToString();
        }

    }
}