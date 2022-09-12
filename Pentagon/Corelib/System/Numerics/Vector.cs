// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

namespace System.Numerics;

/// <summary>Provides a collection of static convenience methods for creating, manipulating, combining, and converting generic vectors.</summary>
public static class Vector
{
    
    internal static void ThrowInsufficientNumberOfElementsException(int requiredElementCount)
    {
        throw new IndexOutOfRangeException("At least {requiredElementCount} element(s) are expected in the parameter \"values\".");
    }

    
}