namespace System.Runtime.CompilerServices;

[Flags]
public enum MethodImplOptions
{
    
    AggressiveInlining = 256,
    AggressiveOptimization = 512,
    ForwardRef = 16,
    InternalCall = 4096,
    NoInlining = 8,
    NoOptimization = 64,
    PreserveSig = 128,
    Synchronized = 32,
    Unmanaged = 4

}