using System.Collections.Generic;
using System.Diagnostics;

namespace Tomato.App;

public class Capability
{
    
    /// <summary>
    /// Used to hold the capabilities by an id 
    /// </summary>
    private static Dictionary<int, Capability> _capById = new();

    /// <summary>
    /// The name of the capability, user readable
    /// </summary>
    public string Name { get; }
    
    /// <summary>
    /// The description of the capability, user readable
    /// </summary>
    public string Description { get; }
    
    /// <summary>
    /// The index of the capability in the capability bitmap
    /// </summary>
    internal int Index { get; }

    /// <summary>
    /// Create a new capability
    ///
    /// TODO: store the current excecuting assembly
    /// </summary>
    /// <param name="name"></param>
    /// <param name="description"></param>
    public Capability(string name, string description)
    {
        Name = name;
        Description = description;

        lock (_capById)
        {
            Index = _capById.Count;
            _capById[Index] = this;
        }
    }
    
}