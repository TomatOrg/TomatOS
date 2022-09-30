using System.Linq.Expressions;
using System.Runtime.Serialization;

namespace System.Dynamic.Utils;

internal static class TypeUtils
{
    
    public static Type GetNonNullableType(this Type type) => IsNullableType(type) ? type.GetGenericArguments()[0] : type;

    public static bool IsNullableType(this Type type) => type.IsConstructedGenericType && type.GetGenericTypeDefinition() == typeof(Nullable<>);

    public static void ValidateType(Type type, string? paramName) => ValidateType(type, paramName, false, false);
    
    public static void ValidateType(Type type, string? paramName, bool allowByRef, bool allowPointer)
    {
        if (ValidateType(type, paramName, -1))
        {
            if (!allowByRef && type.IsByRef)
            {
                throw Error.TypeMustNotBeByRef(paramName);
            }

            if (!allowPointer && type.IsPointer)
            {
                throw Error.TypeMustNotBePointer(paramName);
            }
        }
    }

    public static bool ValidateType(Type type, string? paramName, int index)
    {
        if (type == typeof(void))
        {
            return false; // Caller can skip further checks.
        }

        if (type.ContainsGenericParameters)
        {
            throw type.IsGenericTypeDefinition
                ? Error.TypeIsGeneric(type, paramName, index)
                : Error.TypeContainsGenericParameters(type, paramName, index);
        }

        return true;
    }
    
    public static bool IsArithmetic(this Type type)
    {
        type = GetNonNullableType(type);
        if (!type.IsEnum)
        {
            switch (type.GetTypeCode())
            {
                case TypeCode.Int16:
                case TypeCode.Int32:
                case TypeCode.Int64:
                case TypeCode.Double:
                case TypeCode.Single:
                case TypeCode.UInt16:
                case TypeCode.UInt32:
                case TypeCode.UInt64:
                    return true;
            }
        }

        return false;
    }

    
}