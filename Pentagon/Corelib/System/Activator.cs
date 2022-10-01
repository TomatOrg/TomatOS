using System.Runtime.CompilerServices;

namespace System;

public static class Activator
{

    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Native)]
    private static extern object InternalCreateInstance(Type type, object[] args);

    public static object CreateInstance(Type type, params object[] args)
    {
        if (type == null)
            throw new ArgumentNullException();

        if (type.ContainsGenericParameters)
            throw new ArgumentException();

        return InternalCreateInstance(type, args);
    }
    
    public static T CreateInstance<T>() 
        where T : class
    {
        return Unsafe.As<T>(CreateInstance(typeof(T)));
    }
    
    // [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    // public static extern T CreateInstance<T>() where T : new();

}