namespace System
{
    public class Object
    {
        private Type _type;
        private byte flags;
        private byte _reserved1;
        private byte _reserved2;
        private byte _reserved3;
        private uint _reserved4;

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