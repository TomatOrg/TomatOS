using System.Runtime.CompilerServices;

namespace Pentagon.DriverServices;

public class Field<T> 
    where T : unmanaged
{

    // keep reference to the region
    private Region _region;
    private ulong _fieldPtr;

    public ref T Value => ref MemoryServices.UnsafePtrToRef<T>(_fieldPtr);
    
    internal Field(Region region, int offset)
    {
        _region = region;
        
        // slice the span to get into the correct offset and then get the raw ptr
        var span = region.Span.Slice(offset, Unsafe.SizeOf<T>());
        _fieldPtr = MemoryServices.GetSpanPtr(ref span);
    }

}