using System;
using System.Runtime.InteropServices;

namespace Pentagon.DriverServices.Pci;

/// <summary>
/// MSI-X controller for a PCI device 
/// </summary>
public class Msix
{

    // The msix capability 
    private Field<Capability.Msix> _capability;
    private Memory<MsixEntry> _table;

    // the IRQs allocated for this structure
    private Irq[] _irqs;
    private int _configuredIrqs = 0;

    /// <summary>
    /// Gets the amount of IRQs that are supported by this MSI-X function
    /// </summary>
    public int Count => _configuredIrqs;

    internal Msix(PciDevice device, Memory<byte> capability)
    {
        // get the cap
        var region = new Region(capability);
        _capability = region.CreateField<Capability.Msix>(0);
        ref var cap = ref _capability.Value;
        
        // get the table
        _table = MemoryMarshal.Cast<byte, MsixEntry>(device.MapBar((int)(cap.Table & 0b111)));
        
        // make sure its disabled at the start
        cap.MessageControl &= ~Capability.Msix.MsgCtrl.Enable;

        // create the irqs table
        _irqs = new Irq[(int)(cap.MessageControl & Capability.Msix.MsgCtrl.TableSizeMask) + 1];
        _configuredIrqs = 0;
        
        // clear the table, masking all the entries
        for (var i = 0; i < _irqs.Length; )
        {
            ref var entry = ref _table.Span[i];
            entry.Addr = 0;
            entry.Ctrl = 0;
            entry.Ctrl = 1;
        }
    }
    
    /// <summary>
    /// Get the wanted irq 
    /// </summary>
    /// <param name="index"></param>
    public Irq this[int index]
    {
        get
        {
            if (index > _configuredIrqs)
                throw new ArgumentOutOfRangeException(nameof(index));
            return _irqs[index];
        }
    }

    /// <summary>
    /// Configure the table with the given number of irqs
    /// TODO: only support to configure once?
    /// </summary>
    public void Configure(int count)
    {
        // TODO: support for freeing allocated irqs 
        //       this is actually complex because the user might still 
        //       have a reference to the IRQ, and might be even waiting
        //       on it!
        if (count < _configuredIrqs)
            throw new InvalidOperationException();

        // stop MSI-X while we are working
        _capability.Value.MessageControl &= ~Capability.Msix.MsgCtrl.Enable;
        
        // now configure all the newly configured irqs 
        var tableBase = MemoryServices.GetMappedPhysicalAddress(MemoryMarshal.Cast<MsixEntry, byte>(_table));
        for (var i = _configuredIrqs; i < count; i++)
        {
            // the vector control is the last dword of a 4 dword structure
            var irq = Irq.AllocateIrq(1, Irq.IrqMaskType.Msix, tableBase + (ulong)i * 4 + 3);
            _irqs[i] = new Irq(irq);
            
            // configure it, we are going to set it as lowest priority cpu, this
            // will allow a cpu that is not working right now to handle it nicely.
            // we keep the entry as masked, the wait will unmask it 
            ref var entry = ref _table.Span[i];
            entry.Addr = 0xFEE00000;
            entry.Data = (uint)((1 << 8) | irq);
        }

        // set the new configured count 
        _configuredIrqs = count;
        
        // enable MSI-X
        _capability.Value.MessageControl |= Capability.Msix.MsgCtrl.Enable;
    }
    
    [StructLayout(LayoutKind.Sequential)]
    private struct MsixEntry
    {
        public ulong Addr;
        public uint Data;
        public uint Ctrl;
    }

}