namespace System;

public struct RuntimeTypeHandle
{
    
    public IntPtr Value { get; }

    internal static object CreateInstanceForAnotherGenericParameter(Type type, Type genericParameter)
    {
        if (!type.IsGenericTypeDefinition)
        {
            type = type.GetGenericTypeDefinition();
        }
        return Activator.CreateInstance(type.MakeGenericType(genericParameter));
    }
    
}