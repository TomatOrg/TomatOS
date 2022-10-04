// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

namespace System.Drawing;

/// <summary>
/// Represents an ordered pair of x and y coordinates that define a point in a two-dimensional plane.
/// </summary>
public struct Point
{
    
    /// <summary>
    /// Creates a new instance of the <see cref='System.Drawing.Point'/> class with member data left uninitialized.
    /// </summary>
    public static readonly Point Empty;

    public int X { get; set; }
    public int Y { get; set; }

    /// <summary>
    /// Gets a value indicating whether this <see cref='System.Drawing.Point'/> is empty.
    /// </summary>
    public readonly bool IsEmpty => X == 0 && Y == 0;

    /// <summary>
    /// Initializes a new instance of the <see cref='System.Drawing.Point'/> class with the specified coordinates.
    /// </summary>
    public Point(int x, int y)
    {
        X = x;
        Y = y;
    }

    /// <summary>
    /// Initializes a new instance of the <see cref='System.Drawing.Point'/> class from a <see cref='System.Drawing.Size'/> .
    /// </summary>
    public Point(Size sz)
    {
        X = sz.Width;
        Y = sz.Height;
    }

    /// <summary>
    /// Creates a <see cref='System.Drawing.PointF'/> with the coordinates of the specified <see cref='System.Drawing.Point'/>
    /// </summary>
    public static implicit operator PointF(Point p) => new(p.X, p.Y);

    /// <summary>
    /// Creates a <see cref='System.Drawing.Size'/> with the coordinates of the specified <see cref='System.Drawing.Point'/> .
    /// </summary>
    public static explicit operator Size(Point p) => new(p.X, p.Y);

}