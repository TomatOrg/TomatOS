using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace System
{
    [StructLayout(LayoutKind.Sequential)]
    public class Object
    {
        private unsafe void* _vtable;
        private byte flags;
        private byte _reserved1;
        private byte _reserved2;
        private byte _reserved3;
        private uint _typeIndex;

        #region Internal state modification

        // TODO: do we want to put this in a critical path? I think it is not needed
        //       since this only will be used once the object is fully destroyed, so 
        //       as long as someone got a reference it is alive or inside the finalizer
        //       which is still considered alive
        
        internal void ReRegisterForFinalize()
        {
            flags |= (1 << 3);
        }

        internal void SuppressFinalize()
        {
            flags &= unchecked((byte)~(1 << 3));
        }

        #endregion
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern Type GetType();
        
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