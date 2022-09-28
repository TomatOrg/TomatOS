using System.Runtime.CompilerServices;

namespace Pentagon.DriverServices;

public static class KernelUtils
{

    #region AlignDown

    public static ulong AlignDown(ulong value, ulong alignment)
    {
        return value - (value & (alignment - 1));
    }
    
    #endregion

    #region AlignUp

    public static ulong AlignUp(ulong value, ulong alignment)
    {
        return (value + (alignment - 1)) & ~(alignment - 1);
    }
    
    #endregion

    #region DivideUp

    public static ulong DivideUp(ulong value, ulong alignment)
    {
        return (value + (alignment - 1)) / alignment;
    }
    
    public static int DivideUp(int value, int alignment)
    {
        return (value + (alignment - 1)) / alignment;
    }

    #endregion
    
    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Native)]
    internal static extern bool GetNextFramebuffer(ref int index, out ulong addr, out int width, out int height, out int pitch);

    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Native)]
    internal static extern void GetKbdLayout(out ulong addr, out ulong size);
}