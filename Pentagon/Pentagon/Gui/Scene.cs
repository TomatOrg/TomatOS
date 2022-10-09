using System.Collections.Generic;
using Pentagon.Gui.Framework;

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