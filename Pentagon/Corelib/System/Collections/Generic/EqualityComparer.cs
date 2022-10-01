// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System.Diagnostics.CodeAnalysis;
using System.Runtime.CompilerServices;

namespace System.Collections.Generic;

internal abstract class EqualityComparer<T> : IEqualityComparer<T>
{
    // To minimize generic instantiation overhead of creating the comparer per type, we keep the generic portion of the code as small
    // as possible and define most of the creation logic in a non-generic class.
    public static EqualityComparer<T> Default { get; } = (EqualityComparer<T>)ComparerHelpers.CreateDefaultEqualityComparer(typeof(T));
    public abstract bool Equals(T? x, T? y);
    public abstract int GetHashCode([DisallowNull] T obj);

}

// The methods in this class look identical to the inherited methods, but the calls
// to Equal bind to IEquatable<T>.Equals(T) instead of Object.Equals(Object)
internal sealed class GenericEqualityComparer<T> : EqualityComparer<T> where T : IEquatable<T>
{
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public override bool Equals(T? x, T? y)
    {
        if (x != null)
        {
            if (y != null) return x.Equals(y);
            return false;
        }
        if (y != null) return false;
        return true;
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public override int GetHashCode([DisallowNull] T obj) => obj?.GetHashCode() ?? 0;

    // Equals method for the comparer itself.
    // If in the future this type is made sealed, change the is check to obj != null && GetType() == obj.GetType().
    public override bool Equals([NotNullWhen(true)] object? obj) =>
        obj is GenericEqualityComparer<T>;

    // If in the future this type is made sealed, change typeof(...) to GetType().
    public override int GetHashCode() =>
        typeof(GenericEqualityComparer<T>).GetHashCode();
}

internal sealed class NullableEqualityComparer<T> : EqualityComparer<T?> 
    where T : struct, IEquatable<T>
{
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public override bool Equals(T? x, T? y)
    {
        if (x.HasValue)
        {
            if (y.HasValue) return x._value.Equals(y._value);
            return false;
        }
        if (y.HasValue) return false;
        return true;
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public override int GetHashCode(T? obj) => obj.GetHashCode();

    // Equals method for the comparer itself.
    public override bool Equals([NotNullWhen(true)] object? obj) =>
        obj != null && GetType() == obj.GetType();

    public override int GetHashCode() =>
        GetType().GetHashCode();
}

internal sealed class ObjectEqualityComparer<T> : EqualityComparer<T>
{
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public override bool Equals(T? x, T? y)
    {
        if (x != null)
        {
            if (y != null) return x.Equals(y);
            return false;
        }
        if (y != null) return false;
        return true;
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public override int GetHashCode([DisallowNull] T obj) => obj?.GetHashCode() ?? 0;

    // Equals method for the comparer itself.
    public override bool Equals([NotNullWhen(true)] object? obj) =>
        obj != null && GetType() == obj.GetType();

    public override int GetHashCode() =>
        GetType().GetHashCode();
}

internal sealed partial class ByteEqualityComparer : EqualityComparer<byte>
{
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public override bool Equals(byte x, byte y)
    {
        return x == y;
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public override int GetHashCode(byte b)
    {
        return b.GetHashCode();
    }

    // Equals method for the comparer itself.
    public override bool Equals([NotNullWhen(true)] object? obj) =>
        obj != null && GetType() == obj.GetType();

    public override int GetHashCode() =>
        GetType().GetHashCode();
}

// Needs to be public to support binary serialization compatibility
internal sealed class EnumEqualityComparer<T> : EqualityComparer<T>
    where T : struct, Enum
{
    public EnumEqualityComparer() { }

    public override bool Equals(T x, T y)
    {
        throw new NotImplementedException();
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public override int GetHashCode(T obj)
    {
        return obj.GetHashCode();
    }

    // Equals method for the comparer itself.
    public override bool Equals([NotNullWhen(true)] object? obj) =>
        obj != null && GetType() == obj.GetType();

    public override int GetHashCode() =>
        GetType().GetHashCode();
}