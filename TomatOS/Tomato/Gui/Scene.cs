using System.Collections.Generic;
using Tomato.Gui.Framework;

namespace Tomato.Gui;

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