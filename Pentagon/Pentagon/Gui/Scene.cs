using System.Collections.Generic;
using System.Linq.Expressions;

namespace Pentagon.Gui;

/// <summary>
/// Represents a GUI scene
/// </summary>
public class Scene
{
    
    /// <summary>
    /// The commands to render the scene
    /// </summary>
    public List<Command> Commands { get; set; }
    
}