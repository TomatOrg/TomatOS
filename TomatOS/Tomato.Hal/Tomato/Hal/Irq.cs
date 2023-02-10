using System.Runtime.CompilerServices;

namespace Tomato.Hal;

public class Irq
{

    /// <summary>
    /// The vector number allocated for this IRQ 
    /// </summary>
    private readonly int _vector;
        
    internal Irq(int vector)
    {
        _vector = vector;
    }

    /// <summary>
    /// Blocks the current thread, waiting for an IRQ to happen
    /// </summary>
    public void Wait()
    {
        IrqWait(_vector);
    }

    #region Native

    /// <summary>
    /// Allocates a new IRQ, this is completely abstracted by interrupt-sources
    /// which are defined by the kernel itself
    /// </summary>
    /// <param name="count">How many to allocate (contig)</param>
    /// <returns>The base number for the irq, or -1 if there are no empty irqs</returns>
    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    internal static extern int AllocateIrq(int count);

    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
    private static extern void IrqWait(int irqNum);

    #endregion
}