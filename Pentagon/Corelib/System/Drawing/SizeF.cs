// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

namespace System.Drawing;

/// <summary>
/// Represents the size of a rectangular region with an ordered pair of width and height.
/// </summary>
public struct SizeF
{
    /// <summary>
    /// Initializes a new instance of the <see cref='System.Drawing.SizeF'/> class.
    /// </summary>
    public static readonly SizeF Empty;
    
    public float Width { get; set; }
    public float Height { get; set; }
    
    /// <summary>
    /// Initializes a new instance of the <see cref='System.Drawing.SizeF'/> class from the specified
    /// existing <see cref='System.Drawing.SizeF'/>.
    /// </summary>
    public SizeF(SizeF size)
    {
        Width = size.Width;
        Height = size.Height;
    }

    /// <summary>
    /// Initializes a new instance of the <see cref='System.Drawing.SizeF'/> class from the specified
    /// <see cref='System.Drawing.PointF'/>.
    /// </summary>
    public SizeF(PointF pt)
    {
        Width = pt.X;
        Height = pt.Y;
    }
    
    /// <summary>
    /// Initializes a new instance of the <see cref='System.Drawing.SizeF'/> class from the specified dimensions.
    /// </summary>
    public SizeF(float width, float height)
    {
        Width = width;
        Height = height;
    }

}