// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System.Diagnostics.CodeAnalysis;
using System.Runtime.CompilerServices;
using System.Runtime.Serialization;

namespace System.Collections.Generic;

public abstract class Comparer<T> : IComparer<T>
{
    // public static Comparer<T> Default is runtime-specific

    public static Comparer<T> Create(Comparison<T> comparison)
    {
        if (comparison == null)
            throw new ArgumentNullException(nameof(comparison));

        return new ComparisonComparer<T>(comparison);
    }

    public abstract int Compare(T? x, T? y);

}

internal sealed class ComparisonComparer<T> : Comparer<T>
{
    private readonly Comparison<T> _comparison;

    public ComparisonComparer(Comparison<T> comparison)
    {
        _comparison = comparison;
    }

    public override int Compare(T? x, T? y) => _comparison(x!, y!);
}

// Note: although there is a lot of shared code in the following
// comparers, we do not incorporate it into a base class for perf
// reasons. Adding another base class (even one with no fields)
// means another generic instantiation, which can be costly esp.
// for value types.
internal sealed class GenericComparer<T> : Comparer<T> 
    where T : IComparable<T>
{
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public override int Compare(T? x, T? y)
    {
        if (x != null)
        {
            if (y != null) return x.CompareTo(y);
            return 1;
        }
        if (y != null) return -1;
        return 0;
    }

    // Equals method for the comparer itself.
    public override bool Equals([NotNullWhen(true)] object? obj) =>
        obj != null && GetType() == obj.GetType();

    public override int GetHashCode() =>
        GetType().GetHashCode();
}

internal sealed class NullableComparer<T> : Comparer<T?> 
    where T : struct, IComparable<T>
{
    public override int Compare(T? x, T? y)
    {
        if (x.HasValue)
        {
            if (y.HasValue) return x._value.CompareTo(y._value);
            return 1;
        }
        if (y.HasValue) return -1;
        return 0;
    }

    // Equals method for the comparer itself.
    public override bool Equals([NotNullWhen(true)] object? obj) =>
        obj != null && GetType() == obj.GetType();

    public override int GetHashCode() =>
        GetType().GetHashCode();
}

public sealed class ObjectComparer<T> : Comparer<T>
{
    public override int Compare(T? x, T? y)
    {
        object? a = x;
        object? b = y;
        if (a == b) return 0;
        if (a == null) return -1;
        if (b == null) return 1;

        throw new ArgumentException("At least one object must implement IComparable.");
    }

    // Equals method for the comparer itself.
    public override bool Equals([NotNullWhen(true)] object? obj) =>
        obj != null && GetType() == obj.GetType();

    public override int GetHashCode() =>
        GetType().GetHashCode();
}

internal sealed class EnumComparer<T> : Comparer<T>
    where T : struct, Enum
{
    public EnumComparer() { }
    
    public override int Compare(T x, T y)
    {
        throw new NotImplementedException();
    }
    
    // Equals method for the comparer itself.
    public override bool Equals([NotNullWhen(true)] object? obj) =>
        obj != null && GetType() == obj.GetType();

    public override int GetHashCode() =>
        GetType().GetHashCode();

}