namespace Pentagon;

public class Kernel
{

    public static int Main()
    {
        using var framebufferOwner = MemoryServices.MapPages(0xFD000000, 10);
        framebufferOwner.Memory.Span.Fill(0xFF);
        return 0;
    }
    
}