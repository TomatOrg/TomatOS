namespace Tomato.Hal.Acpi.Resource;

public enum InterruptPolarity
{
    /// <summary>
    /// This interrupt is sampled when the signal is high, or true
    /// </summary>
    ActiveHigh,
    
    /// <summary>
    /// This interrupt is sampled when the signal is low, or false
    /// </summary>
    ActiveLow,
}

public enum InterruptMode
{
    /// <summary>
    /// Interrupt is triggered in response to signal in low state.
    /// </summary>
    LevelTriggered,
    
    /// <summary>
    /// Interrupt is triggered in response to a change in signal state
    /// from low to high
    /// </summary>
    EdgeTriggered,
}

public class IrqResource
{

    /// <summary>
    /// If true the interrupt is capable of waking the system from a
    /// low-power idle state or a system sleep state.
    /// </summary>
    public bool WakeCapable { get; }
    
    /// <summary>
    /// If true the interrupt is shared with other devices
    /// </summary>
    public bool Sharing { get; }
    
    public InterruptPolarity Polarity { get; }
    public InterruptMode Mode { get; }

    /// <summary>
    /// The IRQ objects that can be used for this device
    /// </summary>
    public Irq[] Irqs { get; }

    internal IrqResource(bool wakeCapable, bool sharing, InterruptPolarity polarity, InterruptMode mode, Irq[] irqs)
    {
        WakeCapable = wakeCapable;
        Sharing = sharing;
        Polarity = polarity;
        Mode = mode;
        Irqs = irqs;
    }
    
    internal IrqResource(Irq[] irqs)
    {
        WakeCapable = false;
        Sharing = false;
        Polarity = InterruptPolarity.ActiveHigh;
        Mode = InterruptMode.EdgeTriggered;
        Irqs = irqs;
    }
    
}