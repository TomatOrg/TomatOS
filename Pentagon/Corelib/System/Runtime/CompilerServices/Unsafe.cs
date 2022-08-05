namespace System.Runtime.CompilerServices;

public static class Unsafe
{

    // TODO: can be optimized by having a jit generated code

    [MethodImpl(MethodImplOptions.AggressiveInlining, MethodCodeType = MethodCodeType.Runtime)]
    public static extern int SizeOf<T>();

    [MethodImpl(MethodImplOptions.AggressiveInlining, MethodCodeType = MethodCodeType.Runtime)]
    internal static extern T As<T>(object o) where T : class;

    [MethodImpl(MethodImplOptions.AggressiveInlining, MethodCodeType = MethodCodeType.Runtime)]
    internal static extern ref TTo As<TFrom, TTo>(ref TFrom source);

}