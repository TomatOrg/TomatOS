namespace System.Runtime.CompilerServices;

public static class Unsafe
{

    // TODO: can be optimized by having a jit generated code

    [MethodImpl(MethodImplOptions.InternalCall | MethodImplOptions.AggressiveInlining)]
    public static extern int SizeOf<T>();

    [MethodImpl(MethodImplOptions.InternalCall | MethodImplOptions.AggressiveInlining)]
    internal static extern T As<T>(object o) where T : class;

    [MethodImpl(MethodImplOptions.InternalCall | MethodImplOptions.AggressiveInlining)]
    internal static extern ref TTo As<TFrom, TTo>(ref TFrom source);

}