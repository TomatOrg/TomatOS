namespace System;

public abstract class Enum : ValueType
{

    public static Type GetUnderlyingType(Type enumType)
    {
        if (enumType == null)
            throw new ArgumentNullException();
        if (!enumType.IsEnum)
            throw new ArgumentException();
        
        return enumType.GetElementType();
    }
    
}