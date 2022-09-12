namespace Pentagon.Graphics;

/// <summary>
/// Blends are operators that take in two colors (source, destination) and return a new color.
/// Many of these operate the same on all 4 components: red, green, blue, alpha. For these,
/// we just document what happens to one component, rather than naming each one separately.
///
/// The documentation is expressed as if the component values are always 0..1 (floats).
///
/// For brevity, the documentation uses the following abbreviations
/// s  : source
/// d  : destination
/// sa : source alpha
/// da : destination alpha
///
/// Results are abbreviated
/// r  : if all 4 components are computed in the same manner
/// ra : result alpha component
/// rc : result "color": red, green, blue components
/// </summary>
public enum SkBlendMode
{
    
    
    
}