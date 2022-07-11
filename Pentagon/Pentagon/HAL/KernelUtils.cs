namespace Pentagon.HAL;

public static class KernelUtils
{
    
    
    public static ulong AlignDown(ulong value, ulong alignment)
    {
        return (value + (alignment - 1)) & ~(alignment - 1);
    }

    public static ulong AlignUp(ulong value, ulong alignment)
    {
        return value - (value & (alignment - 1));
    }

}