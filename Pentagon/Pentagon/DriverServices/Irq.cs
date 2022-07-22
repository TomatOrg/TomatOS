using System.Runtime.CompilerServices;

namespace Pentagon.DriverServices;

public class Irq
{

    /// <summary>
    /// The vector number allocated for this IRQ 
    /// </summary>
    public readonly int Vector;
        
    internal Irq(int vector)
    {
        Vector = vector;
    }

    /// <summary>
    /// Blocks the current thread, waiting for an IRQ to happen
    /// </summary>
    public void Wait()
    {
        IrqWait(Vector);
    }

    #region Native

             
    internal enum IrqMaskType
    {
        /// <summary>
        /// The address should point to the vector control dword in the msix table
        /// </summary>
        Msix,
            
        // TODO: MSI support
    }

    /// <summary>
    /// Allocates a new IRQ, this is completely abstracted by interrupt-sources
    /// which are defined by the kernel itself
    /// </summary>
    /// <param name="count">How many to allocate (contig)</param>
    /// <param name="type">How to mask the irq</param>
    /// <param name="addr">The address used for masking</param>
    /// <returns>The base number for the irq, or -1 if there are no empty irqs</returns>
    [MethodImpl(MethodImplOptions.InternalCall)]
    internal static extern int AllocateIrq(int count, IrqMaskType type, ulong addr);

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern void IrqWait(int irqNum);

    #endregion
}