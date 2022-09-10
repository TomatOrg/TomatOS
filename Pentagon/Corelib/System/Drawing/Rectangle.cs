namespace System.Drawing;

/// <summary>
/// Stores the location and size of a rectangular region.
/// </summary>
public struct Rectangle
{
    
    public static readonly Rectangle Empty;

    public int X { get; set; }
    public int Y { get; set; }
    public int Width { get; set; }
    public int Height { get; set; }
    
    /// <summary>
    /// Initializes a new instance of the <see cref='System.Drawing.Rectangle'/> class with the specified location
    /// and size.
    /// </summary>
    public Rectangle(int x, int y, int width, int height)
    {
        X = x;
        Y = y;
        Width = width;
        Height = height;
    }

    /// <summary>
    /// Initializes a new instance of the Rectangle class with the specified location and size.
    /// </summary>
    public Rectangle(Point location, Size size)
    {
        X = location.X;
        Y = location.Y;
        Width = size.Width;
        Height = size.Height;
    }

    /// <summary>
    /// Creates a new <see cref='System.Drawing.Rectangle'/> with the specified location and size.
    /// </summary>
    public static Rectangle FromLTRB(int left, int top, int right, int bottom) =>
        new Rectangle(left, top, unchecked(right - left), unchecked(bottom - top));
    
    /// <summary>
    /// Gets or sets the coordinates of the upper-left corner of the rectangular region represented by this
    /// <see cref='System.Drawing.Rectangle'/>.
    /// </summary>
    public Point Location
    {
        readonly get => new(X, Y);
        set
        {
            X = value.X;
            Y = value.Y;
        }
    }

    /// <summary>
    /// Gets or sets the size of this <see cref='System.Drawing.Rectangle'/>.
    /// </summary>
    public Size Size
    {
        readonly get => new(Width, Height);
        set
        {
            Width = value.Width;
            Height = value.Height;
        }
    }
    
    /// <summary>
    /// Gets the x-coordinate of the upper-left corner of the rectangular region defined by this
    /// <see cref='System.Drawing.Rectangle'/> .
    /// </summary>
    public readonly int Left => X;

    /// <summary>
    /// Gets the y-coordinate of the upper-left corner of the rectangular region defined by this
    /// <see cref='System.Drawing.Rectangle'/>.
    /// </summary>
    public readonly int Top => Y;

    /// <summary>
    /// Gets the x-coordinate of the lower-right corner of the rectangular region defined by this
    /// <see cref='System.Drawing.Rectangle'/>.
    /// </summary>
    public readonly int Right => unchecked(X + Width);

    /// <summary>
    /// Gets the y-coordinate of the lower-right corner of the rectangular region defined by this
    /// <see cref='System.Drawing.Rectangle'/>.
    /// </summary>
    public readonly int Bottom => unchecked(Y + Height);

    /// <summary>
    /// Tests whether this <see cref='System.Drawing.Rectangle'/> has a <see cref='System.Drawing.Rectangle.Width'/>
    /// or a <see cref='System.Drawing.Rectangle.Height'/> of 0.
    /// </summary>
    public readonly bool IsEmpty => Height == 0 && Width == 0 && X == 0 && Y == 0;

}