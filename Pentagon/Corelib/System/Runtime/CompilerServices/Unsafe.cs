namespace System.Runtime.CompilerServices;

public static class Unsafe
{

    // TODO: can be optimized by having a jit generated code

    [MethodImpl(MethodImplOptions.InternalCall)]
    public static extern int SizeOf<T>();

}