using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace System
{
    [StructLayout(LayoutKind.Sequential)]
    public class Object
    {
        private unsafe void* _vtable;
        private ulong _typeFlags;

        #region Internal state modification

        // TODO: do we want to put this in a critical path? I think it is not needed
        //       since this only will be used once the object is fully destroyed, so 
        //       as long as someone got a reference it is alive or inside the finalizer
        //       which is still considered alive
        
        internal void ReRegisterForFinalize()
        {
            _typeFlags |= (1ul << 51);
        }

        internal void SuppressFinalize()
        {
            _typeFlags &= unchecked((byte)~(1ul << 51));
        }

        #endregion
        
        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
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
            return RuntimeHelpers.GetHashCode(this);
        }

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Native)]
        protected extern object MemberwiseClone();
        
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