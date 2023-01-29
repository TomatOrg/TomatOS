using System;
using System.Threading;

namespace Tomato.App;

public class CapabilityDomain : IDisposable
{

    /// <summary>
    /// Holds the domain of the current task
    /// </summary>
    private static AsyncLocal<CapabilityDomain> _current = new();

    /// <summary>
    /// Get/Set the app domain of the current app
    /// </summary>
    public static CapabilityDomain Current
    {
        get
        {
            var current = _current.Value;
            if (current == null)
            {
                current = App.Current._defaultCapabilities;
                _current.Value = current;
            }
            return current;
        }
        set => _current.Value = value;
    }

    internal static CapabilityDomain CreateRoot()
    {
        var cap = new CapabilityDomain();
        cap._capableBitmap = ulong.MaxValue;
        cap._sharableBitmap = ulong.MaxValue;
        return cap;
    }

    /// <summary>
    /// Ensures the caller has the given capability
    /// </summary>
    /// <param name="capability">The capability to check</param>
    public static void Ensure(Capability capability)
    {
        Current.Ensure(capability, false);
    }
    
    /// <summary>
    /// The bits this domain has 
    /// </summary>
    private ulong _capableBitmap = 0;

    /// <summary>
    /// The bits this domain can share with other domains, if a bit is
    /// set in here it must also be set in the capable
    /// </summary>
    private ulong _sharableBitmap = 0;

    public CapabilityDomain()
    {
    }

    /// <summary>
    /// Grant the given domain a new capability, the current domain
    /// must have the capability to set this capability
    /// </summary>
    /// <param name="capability">The capability to pass</param>
    /// <param name="sharable">Should the domain be able to grant it as well</param>
    public void Grant(Capability capability, bool sharable = false)
    {
        Ensure(capability, true);
        _capableBitmap |= 1ul << capability.Index;
        if (sharable)
        {
            _sharableBitmap |= 1ul << capability.Index;
        }
    }

    /// <summary>
    /// Check that the given domain has the capability
    /// </summary>
    /// <param name="capability">The capability to check</param>
    /// <param name="sharable">Should we also check if its sharable</param>
    /// <returns></returns>
    public bool Check(Capability capability, bool sharable = false)
    {
        return ((sharable ? _sharableBitmap : _capableBitmap) & (1ul << capability.Index)) != 0;
    }

    /// <summary>
    /// Ensures the domain has the given 
    /// </summary>
    /// <param name="capability">The capability to ensure</param>
    /// <param name="sharable">Should we also check if its sharable</param>
    /// <exception cref="InvalidOperationException">If the capability is not present for domain</exception>
    public void Ensure(Capability capability, bool sharable)
    {
        if (!Check(capability, sharable))
        {
            // TODO: make user friendly
            throw new InvalidOperationException();
        }
    }
    
    /// <summary>
    /// On dispose we clear out all the capabilities
    /// so this domain can no longer do anything even if leaked
    /// </summary>
    public void Dispose()
    {
        _capableBitmap = 0;
        _sharableBitmap = 0;
    }

}