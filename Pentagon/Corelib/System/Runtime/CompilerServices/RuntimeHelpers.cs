namespace System.Runtime.CompilerServices;

public static class RuntimeHelpers
{
    
    [MethodImpl(MethodImplOptions.AggressiveInlining, MethodCodeType = MethodCodeType.Runtime)]
    public static extern bool IsReferenceOrContainsReferences<T>();
    
}