// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

namespace System.Drawing;

/// <summary>
/// Represents the size of a rectangular region with an ordered pair of width and height.
/// </summary>
public struct Size
{
    /// <summary>
    /// Initializes a new instance of the <see cref='System.Drawing.Size'/> class.
    /// </summary>
    public static readonly Size Empty;

    public int Width { get; set; }
    public int Height { get; set; }

    /// <summary>
    /// Tests whether this <see cref='System.Drawing.Size'/> has zero width and height.
    /// </summary>
    public readonly bool IsEmpty => Width == 0 && Height == 0;

    /// <summary>
    /// Initializes a new instance of the <see cref='System.Drawing.Size'/> class from the specified
    /// <see cref='System.Drawing.Point'/>.
    /// </summary>
    public Size(Point pt)
    {
        Width = pt.X;
        Height = pt.Y;
    }
    
    /// <summary>
    /// Initializes a new instance of the <see cref='System.Drawing.Size'/> class from the specified dimensions.
    /// </summary>
    public Size(int width, int height)
    {
        Width = width;
        Height = height;
    }

    /// <summary>
    /// Converts the specified <see cref='System.Drawing.Size'/> to a <see cref='System.Drawing.SizeF'/>.
    /// </summary>
    public static implicit operator SizeF(Size p) => new SizeF(p.Width, p.Height);

    
    
}