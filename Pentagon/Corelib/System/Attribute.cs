using System.Collections.Generic;
using System.Reflection;
using System.Runtime.CompilerServices;

namespace System
{
    public abstract class Attribute
    {


        [MethodImpl(MethodCodeType = MethodCodeType.Native)]
        private static extern Attribute GetCustomAttributeNative(object element, Type attributeType, ref int index);

        private static Attribute GetCustomAttributeInternal(object element, Type attributeType)
        {
            if (element == null)
                throw new ArgumentNullException(nameof(element));
            
            if (attributeType == null)
                throw new ArgumentNullException(nameof(attributeType));
            
            if (attributeType.IsSubclassOf(typeof(Attribute)))
                throw new ArgumentException("Type passed in must be derived from System.Attribute or System.Attribute itself.", nameof(attributeType));

            // get the actual item
            var i = 0;
            var result = GetCustomAttributeNative(element, attributeType, ref i);
            if (result == null)
            {
                return null;
            }
            
            // make sure there are no more items
            i++;
            var result2 = GetCustomAttributeNative(element, attributeType, ref i);
            if (result2 != null)
            {
                throw new AmbiguousMatchException();
            }

            return result;
        }

        private static Attribute[] GetCustomAttributesInternal(object element, Type attributeType)
        {
            if (element == null)
                throw new ArgumentNullException(nameof(element));
            
            if (attributeType == null)
                throw new ArgumentNullException(nameof(attributeType));

            if (!attributeType.IsSubclassOf(typeof(Attribute)))
                throw new ArgumentException("Type passed in must be derived from System.Attribute or System.Attribute itself.", nameof(attributeType));
            
            var attributes = new List<Attribute>();

            var i = 0;
            while (true)
            {
                var result = GetCustomAttributeNative(element, attributeType, ref i);
                if (result == null)
                    break;
                
                attributes.Add(result);
                i++;
            }

            return attributes.ToArray();
        }

        public static Attribute GetCustomAttribute(ParameterInfo element, Type attributeType, bool inherit = true)
        {
            return GetCustomAttributeInternal(element, attributeType);
        }

        public static Attribute GetCustomAttribute(MemberInfo element, Type attributeType, bool inherit = true)
        {
            return GetCustomAttributeInternal(element, attributeType);
        }

        public static Attribute GetCustomAttribute(Assembly element, Type attributeType, bool inherit = true)
        {
            return GetCustomAttributeInternal(element, attributeType);
        }

        public static Attribute GetCustomAttribute(Module element, Type attributeType, bool inherit = true)
        {
            return GetCustomAttributeInternal(element, attributeType);
        }

        public static Attribute[] GetCustomAttributes(ParameterInfo element, Type attributeType, bool inherit = true)
        {
            return GetCustomAttributesInternal(element, attributeType);
        }

        public static Attribute[] GetCustomAttributes(MemberInfo element, Type attributeType, bool inherit = true)
        {
            return GetCustomAttributesInternal(element, attributeType);
        }

        public static Attribute[] GetCustomAttributes(Assembly element, Type attributeType, bool inherit = true)
        {
            return GetCustomAttributesInternal(element, attributeType);
        }

        public static Attribute[] GetCustomAttributes(Module element, Type attributeType, bool inherit = true)
        {
            return GetCustomAttributesInternal(element, attributeType);
        }
    }
}