using System.Runtime.CompilerServices;

namespace System;

public static class Activator
{

    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    public static extern object CreateInstance(Type type, params object[] args);
    
    // [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    // public static extern T CreateInstance<T>() where T : new();

}