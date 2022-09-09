using System.Threading.Tasks;

namespace Pentagon.Interfaces;

public interface ISurface
{
    
    /// <summary>
    /// The width of the surface
    /// </summary>
    public int Width { get; }
    
    /// <summary>
    /// The height of the surface
    /// </summary>
    public int Height { get; }
    
    /// <summary>
    /// The canvas of the surface
    /// </summary>
    public ICanvas Canvas { get; }

    /// <summary>
    /// Draws surface contents to canvas, with top-left corner at (x-y)
    /// </summary>
    public void Draw(ICanvas canvas, float x, float y);

    /// <summary>
    /// Creates a sufrace that can be drawn to the this surface in an
    /// accelerated manner.
    /// </summary>
    public ISurface CreateSubSurface(int width, int height);

}