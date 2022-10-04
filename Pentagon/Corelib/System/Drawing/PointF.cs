// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

namespace System.Drawing;

/// <summary>
/// Represents an ordered pair of x and y coordinates that define a point in a two-dimensional plane.
/// </summary>
public struct PointF
{
    /// <summary>
    /// Creates a new instance of the <see cref='System.Drawing.PointF'/> class with member data left uninitialized.
    /// </summary>
    public static readonly PointF Empty;
    
    public float X { get; set; }
    public float Y { get; set; }

    /// <summary>
    /// Initializes a new instance of the <see cref='System.Drawing.PointF'/> class with the specified coordinates.
    /// </summary>
    public PointF(float x, float y)
    {
        X = x;
        Y = y;
    }

    /// <summary>
    /// Gets a value indicating whether this <see cref='System.Drawing.PointF'/> is empty.
    /// </summary>
    public readonly bool IsEmpty => X == 0f && Y == 0f;

    
    
}