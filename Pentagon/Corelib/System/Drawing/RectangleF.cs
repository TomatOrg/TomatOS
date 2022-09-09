// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

namespace System.Drawing;

/// <summary>
/// Stores the location and size of a rectangular region.
/// </summary>
public struct RectangleF
{
    
    /// <summary>
    /// Initializes a new instance of the <see cref='System.Drawing.RectangleF'/> class.
    /// </summary>
    public static readonly RectangleF Empty;

    public float X { get; set; }
    public float Y { get; set; }
    public float Width { get; set; }
    public float Height { get; set; }

    /// <summary>
    /// Gets or sets the coordinates of the upper-left corner of the rectangular region represented by this
    /// <see cref='System.Drawing.RectangleF'/>.
    /// </summary>
    public PointF Location
    {
        readonly get => new(X, Y);
        set
        {
            X = value.X;
            Y = value.Y;
        }
    }
    
    /// <summary>
    /// Gets or sets the size of this <see cref='System.Drawing.RectangleF'/>.
    /// </summary>
    public SizeF Size
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
    /// <see cref='System.Drawing.RectangleF'/> .
    /// </summary>
    public readonly float Left => X;

    /// <summary>
    /// Gets the y-coordinate of the upper-left corner of the rectangular region defined by this
    /// <see cref='System.Drawing.RectangleF'/>.
    /// </summary>
    public readonly float Top => Y;

    /// <summary>
    /// Gets the x-coordinate of the lower-right corner of the rectangular region defined by this
    /// <see cref='System.Drawing.RectangleF'/>.
    /// </summary>
    public readonly float Right => X + Width;

    /// <summary>
    /// Gets the y-coordinate of the lower-right corner of the rectangular region defined by this
    /// <see cref='System.Drawing.RectangleF'/>.
    /// </summary>
    public readonly float Bottom => Y + Height;

    /// <summary>
    /// Tests whether this <see cref='System.Drawing.RectangleF'/> has a <see cref='System.Drawing.RectangleF.Width'/> or a <see cref='System.Drawing.RectangleF.Height'/> of 0.
    /// </summary>
    public readonly bool IsEmpty => (Width <= 0) || (Height <= 0);

    /// <summary>
    /// Initializes a new instance of the <see cref='System.Drawing.RectangleF'/> class with the specified location
    /// and size.
    /// </summary>
    public RectangleF(float x, float y, float width, float height)
    {
        X = x;
        Y = y;
        Width = width;
        Height = height;
    }

    /// <summary>
    /// Initializes a new instance of the <see cref='System.Drawing.RectangleF'/> class with the specified location
    /// and size.
    /// </summary>
    public RectangleF(PointF location, SizeF size)
    {
        X = location.X;
        Y = location.Y;
        Width = size.Width;
        Height = size.Height;
    }
    
    /// <summary>
    /// Creates a new <see cref='System.Drawing.RectangleF'/> with the specified location and size.
    /// </summary>
    public static RectangleF FromLTRB(float left, float top, float right, float bottom) =>
        new RectangleF(left, top, right - left, bottom - top);

    
    
    
}