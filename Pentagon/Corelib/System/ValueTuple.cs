// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

#pragma warning disable SA1141 // explicitly not using tuple syntax in tuple implementation

using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.Diagnostics.CodeAnalysis;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;

namespace System;

/// <summary>
/// The ValueTuple types (from arity 0 to 8) comprise the runtime implementation that underlies tuples in C# and struct tuples in F#.
/// Aside from created via language syntax, they are most easily created via the ValueTuple.Create factory methods.
/// The System.ValueTuple types differ from the System.Tuple types in that:
/// - they are structs rather than classes,
/// - they are mutable rather than readonly, and
/// - their members (such as Item1, Item2, etc) are fields rather than properties.
/// </summary>
public struct ValueTuple
{
    /// <summary>
    /// Returns a value that indicates whether the current <see cref="ValueTuple"/> instance is equal to a specified object.
    /// </summary>
    /// <param name="obj">The object to compare with this instance.</param>
    /// <returns><see langword="true"/> if <paramref name="obj"/> is a <see cref="ValueTuple"/>.</returns>
    public override bool Equals([NotNullWhen(true)] object? obj)
    {
        return obj is ValueTuple;
    }

    /// <summary>Returns a value indicating whether this instance is equal to a specified value.</summary>
    /// <param name="other">An instance to compare to this instance.</param>
    /// <returns>true if <paramref name="other"/> has the same value as this instance; otherwise, false.</returns>
    public bool Equals(ValueTuple other)
    {
        return true;
    }

    /// <summary>Compares this instance to a specified instance and returns an indication of their relative values.</summary>
    /// <param name="other">An instance to compare.</param>
    /// <returns>
    /// A signed number indicating the relative values of this instance and <paramref name="other"/>.
    /// Returns less than zero if this instance is less than <paramref name="other"/>, zero if this
    /// instance is equal to <paramref name="other"/>, and greater than zero if this instance is greater
    /// than <paramref name="other"/>.
    /// </returns>
    public int CompareTo(ValueTuple other)
    {
        return 0;
    }

    /// <summary>Returns the hash code for this instance.</summary>
    /// <returns>A 32-bit signed integer hash code.</returns>
    public override int GetHashCode()
    {
        return 0;
    }

    /// <summary>
    /// Returns a string that represents the value of this <see cref="ValueTuple"/> instance.
    /// </summary>
    /// <returns>The string representation of this <see cref="ValueTuple"/> instance.</returns>
    /// <remarks>
    /// The string returned by this method takes the form <c>()</c>.
    /// </remarks>
    public override string ToString()
    {
        return "()";
    }

    /// <summary>Creates a new struct 0-tuple.</summary>
    /// <returns>A 0-tuple.</returns>
    public static ValueTuple Create() =>
        default;

    /// <summary>Creates a new struct 1-tuple, or singleton.</summary>
    /// <typeparam name="T1">The type of the first component of the tuple.</typeparam>
    /// <param name="item1">The value of the first component of the tuple.</param>
    /// <returns>A 1-tuple (singleton) whose value is (item1).</returns>
    public static ValueTuple<T1> Create<T1>(T1 item1) =>
        new ValueTuple<T1>(item1);

    /// <summary>Creates a new struct 2-tuple, or pair.</summary>
    /// <typeparam name="T1">The type of the first component of the tuple.</typeparam>
    /// <typeparam name="T2">The type of the second component of the tuple.</typeparam>
    /// <param name="item1">The value of the first component of the tuple.</param>
    /// <param name="item2">The value of the second component of the tuple.</param>
    /// <returns>A 2-tuple (pair) whose value is (item1, item2).</returns>
    public static ValueTuple<T1, T2> Create<T1, T2>(T1 item1, T2 item2) =>
        new ValueTuple<T1, T2>(item1, item2);

    /// <summary>Creates a new struct 3-tuple, or triple.</summary>
    /// <typeparam name="T1">The type of the first component of the tuple.</typeparam>
    /// <typeparam name="T2">The type of the second component of the tuple.</typeparam>
    /// <typeparam name="T3">The type of the third component of the tuple.</typeparam>
    /// <param name="item1">The value of the first component of the tuple.</param>
    /// <param name="item2">The value of the second component of the tuple.</param>
    /// <param name="item3">The value of the third component of the tuple.</param>
    /// <returns>A 3-tuple (triple) whose value is (item1, item2, item3).</returns>
    public static ValueTuple<T1, T2, T3> Create<T1, T2, T3>(T1 item1, T2 item2, T3 item3) =>
        new ValueTuple<T1, T2, T3>(item1, item2, item3);

    /// <summary>Creates a new struct 4-tuple, or quadruple.</summary>
    /// <typeparam name="T1">The type of the first component of the tuple.</typeparam>
    /// <typeparam name="T2">The type of the second component of the tuple.</typeparam>
    /// <typeparam name="T3">The type of the third component of the tuple.</typeparam>
    /// <typeparam name="T4">The type of the fourth component of the tuple.</typeparam>
    /// <param name="item1">The value of the first component of the tuple.</param>
    /// <param name="item2">The value of the second component of the tuple.</param>
    /// <param name="item3">The value of the third component of the tuple.</param>
    /// <param name="item4">The value of the fourth component of the tuple.</param>
    /// <returns>A 4-tuple (quadruple) whose value is (item1, item2, item3, item4).</returns>
    public static ValueTuple<T1, T2, T3, T4> Create<T1, T2, T3, T4>(T1 item1, T2 item2, T3 item3, T4 item4) =>
        new ValueTuple<T1, T2, T3, T4>(item1, item2, item3, item4);

    /// <summary>Creates a new struct 5-tuple, or quintuple.</summary>
    /// <typeparam name="T1">The type of the first component of the tuple.</typeparam>
    /// <typeparam name="T2">The type of the second component of the tuple.</typeparam>
    /// <typeparam name="T3">The type of the third component of the tuple.</typeparam>
    /// <typeparam name="T4">The type of the fourth component of the tuple.</typeparam>
    /// <typeparam name="T5">The type of the fifth component of the tuple.</typeparam>
    /// <param name="item1">The value of the first component of the tuple.</param>
    /// <param name="item2">The value of the second component of the tuple.</param>
    /// <param name="item3">The value of the third component of the tuple.</param>
    /// <param name="item4">The value of the fourth component of the tuple.</param>
    /// <param name="item5">The value of the fifth component of the tuple.</param>
    /// <returns>A 5-tuple (quintuple) whose value is (item1, item2, item3, item4, item5).</returns>
    public static ValueTuple<T1, T2, T3, T4, T5> Create<T1, T2, T3, T4, T5>(T1 item1, T2 item2, T3 item3, T4 item4, T5 item5) =>
        new ValueTuple<T1, T2, T3, T4, T5>(item1, item2, item3, item4, item5);

    /// <summary>Creates a new struct 6-tuple, or sextuple.</summary>
    /// <typeparam name="T1">The type of the first component of the tuple.</typeparam>
    /// <typeparam name="T2">The type of the second component of the tuple.</typeparam>
    /// <typeparam name="T3">The type of the third component of the tuple.</typeparam>
    /// <typeparam name="T4">The type of the fourth component of the tuple.</typeparam>
    /// <typeparam name="T5">The type of the fifth component of the tuple.</typeparam>
    /// <typeparam name="T6">The type of the sixth component of the tuple.</typeparam>
    /// <param name="item1">The value of the first component of the tuple.</param>
    /// <param name="item2">The value of the second component of the tuple.</param>
    /// <param name="item3">The value of the third component of the tuple.</param>
    /// <param name="item4">The value of the fourth component of the tuple.</param>
    /// <param name="item5">The value of the fifth component of the tuple.</param>
    /// <param name="item6">The value of the sixth component of the tuple.</param>
    /// <returns>A 6-tuple (sextuple) whose value is (item1, item2, item3, item4, item5, item6).</returns>
    public static ValueTuple<T1, T2, T3, T4, T5, T6> Create<T1, T2, T3, T4, T5, T6>(T1 item1, T2 item2, T3 item3, T4 item4, T5 item5, T6 item6) =>
        new ValueTuple<T1, T2, T3, T4, T5, T6>(item1, item2, item3, item4, item5, item6);

    /// <summary>Creates a new struct 7-tuple, or septuple.</summary>
    /// <typeparam name="T1">The type of the first component of the tuple.</typeparam>
    /// <typeparam name="T2">The type of the second component of the tuple.</typeparam>
    /// <typeparam name="T3">The type of the third component of the tuple.</typeparam>
    /// <typeparam name="T4">The type of the fourth component of the tuple.</typeparam>
    /// <typeparam name="T5">The type of the fifth component of the tuple.</typeparam>
    /// <typeparam name="T6">The type of the sixth component of the tuple.</typeparam>
    /// <typeparam name="T7">The type of the seventh component of the tuple.</typeparam>
    /// <param name="item1">The value of the first component of the tuple.</param>
    /// <param name="item2">The value of the second component of the tuple.</param>
    /// <param name="item3">The value of the third component of the tuple.</param>
    /// <param name="item4">The value of the fourth component of the tuple.</param>
    /// <param name="item5">The value of the fifth component of the tuple.</param>
    /// <param name="item6">The value of the sixth component of the tuple.</param>
    /// <param name="item7">The value of the seventh component of the tuple.</param>
    /// <returns>A 7-tuple (septuple) whose value is (item1, item2, item3, item4, item5, item6, item7).</returns>
    public static ValueTuple<T1, T2, T3, T4, T5, T6, T7> Create<T1, T2, T3, T4, T5, T6, T7>(T1 item1, T2 item2, T3 item3, T4 item4, T5 item5, T6 item6, T7 item7) =>
        new ValueTuple<T1, T2, T3, T4, T5, T6, T7>(item1, item2, item3, item4, item5, item6, item7);

}

/// <summary>Represents a 1-tuple, or singleton, as a value type.</summary>
/// <typeparam name="T1">The type of the tuple's only component.</typeparam>
public struct ValueTuple<T1>
{
    /// <summary>
    /// The current <see cref="ValueTuple{T1}"/> instance's first component.
    /// </summary>
    public T1 Item1;

    /// <summary>
    /// Initializes a new instance of the <see cref="ValueTuple{T1}"/> value type.
    /// </summary>
    /// <param name="item1">The value of the tuple's first component.</param>
    public ValueTuple(T1 item1)
    {
        Item1 = item1;
    }

    /// <summary>
    /// Returns the hash code for the current <see cref="ValueTuple{T1}"/> instance.
    /// </summary>
    /// <returns>A 32-bit signed integer hash code.</returns>
    public override int GetHashCode()
    {
        return Item1?.GetHashCode() ?? 0;
    }

    /// <summary>
    /// Returns a string that represents the value of this <see cref="ValueTuple{T1}"/> instance.
    /// </summary>
    /// <returns>The string representation of this <see cref="ValueTuple{T1}"/> instance.</returns>
    /// <remarks>
    /// The string returned by this method takes the form <c>(Item1)</c>,
    /// where <c>Item1</c> represents the value of <see cref="Item1"/>. If the field is <see langword="null"/>,
    /// it is represented as <see cref="string.Empty"/>.
    /// </remarks>
    public override string ToString()
    {
        return "(" + Item1?.ToString() + ")";
    }
}

/// <summary>
/// Represents a 2-tuple, or pair, as a value type.
/// </summary>
/// <typeparam name="T1">The type of the tuple's first component.</typeparam>
/// <typeparam name="T2">The type of the tuple's second component.</typeparam>
public struct ValueTuple<T1, T2>
{
    /// <summary>
    /// The current <see cref="ValueTuple{T1, T2}"/> instance's first component.
    /// </summary>
    public T1 Item1;

    /// <summary>
    /// The current <see cref="ValueTuple{T1, T2}"/> instance's second component.
    /// </summary>
    public T2 Item2;

    /// <summary>
    /// Initializes a new instance of the <see cref="ValueTuple{T1, T2}"/> value type.
    /// </summary>
    /// <param name="item1">The value of the tuple's first component.</param>
    /// <param name="item2">The value of the tuple's second component.</param>
    public ValueTuple(T1 item1, T2 item2)
    {
        Item1 = item1;
        Item2 = item2;
    }

    /// <summary>
    /// Returns a string that represents the value of this <see cref="ValueTuple{T1, T2}"/> instance.
    /// </summary>
    /// <returns>The string representation of this <see cref="ValueTuple{T1, T2}"/> instance.</returns>
    /// <remarks>
    /// The string returned by this method takes the form <c>(Item1, Item2)</c>,
    /// where <c>Item1</c> and <c>Item2</c> represent the values of the <see cref="Item1"/>
    /// and <see cref="Item2"/> fields. If either field value is <see langword="null"/>,
    /// it is represented as <see cref="string.Empty"/>.
    /// </remarks>
    public override string ToString()
    {
        return "(" + Item1?.ToString() + ", " + Item2?.ToString() + ")";
    }
}

/// <summary>
/// Represents a 3-tuple, or triple, as a value type.
/// </summary>
/// <typeparam name="T1">The type of the tuple's first component.</typeparam>
/// <typeparam name="T2">The type of the tuple's second component.</typeparam>
/// <typeparam name="T3">The type of the tuple's third component.</typeparam>
public struct ValueTuple<T1, T2, T3>
{
    /// <summary>
    /// The current <see cref="ValueTuple{T1, T2, T3}"/> instance's first component.
    /// </summary>
    public T1 Item1;
    /// <summary>
    /// The current <see cref="ValueTuple{T1, T2, T3}"/> instance's second component.
    /// </summary>
    public T2 Item2;
    /// <summary>
    /// The current <see cref="ValueTuple{T1, T2, T3}"/> instance's third component.
    /// </summary>
    public T3 Item3;

    /// <summary>
    /// Initializes a new instance of the <see cref="ValueTuple{T1, T2, T3}"/> value type.
    /// </summary>
    /// <param name="item1">The value of the tuple's first component.</param>
    /// <param name="item2">The value of the tuple's second component.</param>
    /// <param name="item3">The value of the tuple's third component.</param>
    public ValueTuple(T1 item1, T2 item2, T3 item3)
    {
        Item1 = item1;
        Item2 = item2;
        Item3 = item3;
    }

    /// <summary>
    /// Returns a string that represents the value of this <see cref="ValueTuple{T1, T2, T3}"/> instance.
    /// </summary>
    /// <returns>The string representation of this <see cref="ValueTuple{T1, T2, T3}"/> instance.</returns>
    /// <remarks>
    /// The string returned by this method takes the form <c>(Item1, Item2, Item3)</c>.
    /// If any field value is <see langword="null"/>, it is represented as <see cref="string.Empty"/>.
    /// </remarks>
    public override string ToString()
    {
        return "(" + Item1?.ToString() + ", " + Item2?.ToString() + ", " + Item3?.ToString() + ")";
    }
}

/// <summary>
/// Represents a 4-tuple, or quadruple, as a value type.
/// </summary>
/// <typeparam name="T1">The type of the tuple's first component.</typeparam>
/// <typeparam name="T2">The type of the tuple's second component.</typeparam>
/// <typeparam name="T3">The type of the tuple's third component.</typeparam>
/// <typeparam name="T4">The type of the tuple's fourth component.</typeparam>
public struct ValueTuple<T1, T2, T3, T4>
{
    /// <summary>
    /// The current <see cref="ValueTuple{T1, T2, T3, T4}"/> instance's first component.
    /// </summary>
    public T1 Item1;
    /// <summary>
    /// The current <see cref="ValueTuple{T1, T2, T3, T4}"/> instance's second component.
    /// </summary>
    public T2 Item2;
    /// <summary>
    /// The current <see cref="ValueTuple{T1, T2, T3, T4}"/> instance's third component.
    /// </summary>
    public T3 Item3;
    /// <summary>
    /// The current <see cref="ValueTuple{T1, T2, T3, T4}"/> instance's fourth component.
    /// </summary>
    public T4 Item4;

    /// <summary>
    /// Initializes a new instance of the <see cref="ValueTuple{T1, T2, T3, T4}"/> value type.
    /// </summary>
    /// <param name="item1">The value of the tuple's first component.</param>
    /// <param name="item2">The value of the tuple's second component.</param>
    /// <param name="item3">The value of the tuple's third component.</param>
    /// <param name="item4">The value of the tuple's fourth component.</param>
    public ValueTuple(T1 item1, T2 item2, T3 item3, T4 item4)
    {
        Item1 = item1;
        Item2 = item2;
        Item3 = item3;
        Item4 = item4;
    }

    /// <summary>
    /// Returns a string that represents the value of this <see cref="ValueTuple{T1, T2, T3, T4}"/> instance.
    /// </summary>
    /// <returns>The string representation of this <see cref="ValueTuple{T1, T2, T3, T4}"/> instance.</returns>
    /// <remarks>
    /// The string returned by this method takes the form <c>(Item1, Item2, Item3, Item4)</c>.
    /// If any field value is <see langword="null"/>, it is represented as <see cref="string.Empty"/>.
    /// </remarks>
    public override string ToString()
    {
        return "(" + Item1?.ToString() + ", " + Item2?.ToString() + ", " + Item3?.ToString() + ", " + Item4?.ToString() + ")";
    }
}

/// <summary>
/// Represents a 5-tuple, or quintuple, as a value type.
/// </summary>
/// <typeparam name="T1">The type of the tuple's first component.</typeparam>
/// <typeparam name="T2">The type of the tuple's second component.</typeparam>
/// <typeparam name="T3">The type of the tuple's third component.</typeparam>
/// <typeparam name="T4">The type of the tuple's fourth component.</typeparam>
/// <typeparam name="T5">The type of the tuple's fifth component.</typeparam>
public struct ValueTuple<T1, T2, T3, T4, T5>
{
    /// <summary>
    /// The current <see cref="ValueTuple{T1, T2, T3, T4, T5}"/> instance's first component.
    /// </summary>
    public T1 Item1;
    /// <summary>
    /// The current <see cref="ValueTuple{T1, T2, T3, T4, T5}"/> instance's second component.
    /// </summary>
    public T2 Item2;
    /// <summary>
    /// The current <see cref="ValueTuple{T1, T2, T3, T4, T5}"/> instance's third component.
    /// </summary>
    public T3 Item3;
    /// <summary>
    /// The current <see cref="ValueTuple{T1, T2, T3, T4, T5}"/> instance's fourth component.
    /// </summary>
    public T4 Item4;
    /// <summary>
    /// The current <see cref="ValueTuple{T1, T2, T3, T4, T5}"/> instance's fifth component.
    /// </summary>
    public T5 Item5;

    /// <summary>
    /// Initializes a new instance of the <see cref="ValueTuple{T1, T2, T3, T4, T5}"/> value type.
    /// </summary>
    /// <param name="item1">The value of the tuple's first component.</param>
    /// <param name="item2">The value of the tuple's second component.</param>
    /// <param name="item3">The value of the tuple's third component.</param>
    /// <param name="item4">The value of the tuple's fourth component.</param>
    /// <param name="item5">The value of the tuple's fifth component.</param>
    public ValueTuple(T1 item1, T2 item2, T3 item3, T4 item4, T5 item5)
    {
        Item1 = item1;
        Item2 = item2;
        Item3 = item3;
        Item4 = item4;
        Item5 = item5;
    }

    /// <summary>
    /// Returns a string that represents the value of this <see cref="ValueTuple{T1, T2, T3, T4, T5}"/> instance.
    /// </summary>
    /// <returns>The string representation of this <see cref="ValueTuple{T1, T2, T3, T4, T5}"/> instance.</returns>
    /// <remarks>
    /// The string returned by this method takes the form <c>(Item1, Item2, Item3, Item4, Item5)</c>.
    /// If any field value is <see langword="null"/>, it is represented as <see cref="string.Empty"/>.
    /// </remarks>
    public override string ToString()
    {
        return "(" + Item1?.ToString() + ", " + Item2?.ToString() + ", " + Item3?.ToString() + ", " + Item4?.ToString() + ", " + Item5?.ToString() + ")";
    }
}

/// <summary>
/// Represents a 6-tuple, or sixtuple, as a value type.
/// </summary>
/// <typeparam name="T1">The type of the tuple's first component.</typeparam>
/// <typeparam name="T2">The type of the tuple's second component.</typeparam>
/// <typeparam name="T3">The type of the tuple's third component.</typeparam>
/// <typeparam name="T4">The type of the tuple's fourth component.</typeparam>
/// <typeparam name="T5">The type of the tuple's fifth component.</typeparam>
/// <typeparam name="T6">The type of the tuple's sixth component.</typeparam>
public struct ValueTuple<T1, T2, T3, T4, T5, T6>
{
    /// <summary>
    /// The current <see cref="ValueTuple{T1, T2, T3, T4, T5, T6}"/> instance's first component.
    /// </summary>
    public T1 Item1;
    /// <summary>
    /// The current <see cref="ValueTuple{T1, T2, T3, T4, T5, T6}"/> instance's second component.
    /// </summary>
    public T2 Item2;
    /// <summary>
    /// The current <see cref="ValueTuple{T1, T2, T3, T4, T5, T6}"/> instance's third component.
    /// </summary>
    public T3 Item3;
    /// <summary>
    /// The current <see cref="ValueTuple{T1, T2, T3, T4, T5, T6}"/> instance's fourth component.
    /// </summary>
    public T4 Item4;
    /// <summary>
    /// The current <see cref="ValueTuple{T1, T2, T3, T4, T5, T6}"/> instance's fifth component.
    /// </summary>
    public T5 Item5;
    /// <summary>
    /// The current <see cref="ValueTuple{T1, T2, T3, T4, T5, T6}"/> instance's sixth component.
    /// </summary>
    public T6 Item6;

    /// <summary>
    /// Initializes a new instance of the <see cref="ValueTuple{T1, T2, T3, T4, T5, T6}"/> value type.
    /// </summary>
    /// <param name="item1">The value of the tuple's first component.</param>
    /// <param name="item2">The value of the tuple's second component.</param>
    /// <param name="item3">The value of the tuple's third component.</param>
    /// <param name="item4">The value of the tuple's fourth component.</param>
    /// <param name="item5">The value of the tuple's fifth component.</param>
    /// <param name="item6">The value of the tuple's sixth component.</param>
    public ValueTuple(T1 item1, T2 item2, T3 item3, T4 item4, T5 item5, T6 item6)
    {
        Item1 = item1;
        Item2 = item2;
        Item3 = item3;
        Item4 = item4;
        Item5 = item5;
        Item6 = item6;
    }

    /// <summary>
    /// Returns a string that represents the value of this <see cref="ValueTuple{T1, T2, T3, T4, T5, T6}"/> instance.
    /// </summary>
    /// <returns>The string representation of this <see cref="ValueTuple{T1, T2, T3, T4, T5, T6}"/> instance.</returns>
    /// <remarks>
    /// The string returned by this method takes the form <c>(Item1, Item2, Item3, Item4, Item5, Item6)</c>.
    /// If any field value is <see langword="null"/>, it is represented as <see cref="string.Empty"/>.
    /// </remarks>
    public override string ToString()
    {
        return "(" + Item1?.ToString() + ", " + Item2?.ToString() + ", " + Item3?.ToString() + ", " + Item4?.ToString() + ", " + Item5?.ToString() + ", " + Item6?.ToString() + ")";
    }
}

/// <summary>
/// Represents a 7-tuple, or sentuple, as a value type.
/// </summary>
/// <typeparam name="T1">The type of the tuple's first component.</typeparam>
/// <typeparam name="T2">The type of the tuple's second component.</typeparam>
/// <typeparam name="T3">The type of the tuple's third component.</typeparam>
/// <typeparam name="T4">The type of the tuple's fourth component.</typeparam>
/// <typeparam name="T5">The type of the tuple's fifth component.</typeparam>
/// <typeparam name="T6">The type of the tuple's sixth component.</typeparam>
/// <typeparam name="T7">The type of the tuple's seventh component.</typeparam>
public struct ValueTuple<T1, T2, T3, T4, T5, T6, T7>
{
    /// <summary>
    /// The current <see cref="ValueTuple{T1, T2, T3, T4, T5, T6, T7}"/> instance's first component.
    /// </summary>
    public T1 Item1;
    /// <summary>
    /// The current <see cref="ValueTuple{T1, T2, T3, T4, T5, T6, T7}"/> instance's second component.
    /// </summary>
    public T2 Item2;
    /// <summary>
    /// The current <see cref="ValueTuple{T1, T2, T3, T4, T5, T6, T7}"/> instance's third component.
    /// </summary>
    public T3 Item3;
    /// <summary>
    /// The current <see cref="ValueTuple{T1, T2, T3, T4, T5, T6, T7}"/> instance's fourth component.
    /// </summary>
    public T4 Item4;
    /// <summary>
    /// The current <see cref="ValueTuple{T1, T2, T3, T4, T5, T6, T7}"/> instance's fifth component.
    /// </summary>
    public T5 Item5;
    /// <summary>
    /// The current <see cref="ValueTuple{T1, T2, T3, T4, T5, T6, T7}"/> instance's sixth component.
    /// </summary>
    public T6 Item6;
    /// <summary>
    /// The current <see cref="ValueTuple{T1, T2, T3, T4, T5, T6, T7}"/> instance's seventh component.
    /// </summary>
    public T7 Item7;

    /// <summary>
    /// Initializes a new instance of the <see cref="ValueTuple{T1, T2, T3, T4, T5, T6, T7}"/> value type.
    /// </summary>
    /// <param name="item1">The value of the tuple's first component.</param>
    /// <param name="item2">The value of the tuple's second component.</param>
    /// <param name="item3">The value of the tuple's third component.</param>
    /// <param name="item4">The value of the tuple's fourth component.</param>
    /// <param name="item5">The value of the tuple's fifth component.</param>
    /// <param name="item6">The value of the tuple's sixth component.</param>
    /// <param name="item7">The value of the tuple's seventh component.</param>
    public ValueTuple(T1 item1, T2 item2, T3 item3, T4 item4, T5 item5, T6 item6, T7 item7)
    {
        Item1 = item1;
        Item2 = item2;
        Item3 = item3;
        Item4 = item4;
        Item5 = item5;
        Item6 = item6;
        Item7 = item7;
    }

    /// <summary>
    /// Returns a string that represents the value of this <see cref="ValueTuple{T1, T2, T3, T4, T5, T6, T7}"/> instance.
    /// </summary>
    /// <returns>The string representation of this <see cref="ValueTuple{T1, T2, T3, T4, T5, T6, T7}"/> instance.</returns>
    /// <remarks>
    /// The string returned by this method takes the form <c>(Item1, Item2, Item3, Item4, Item5, Item6, Item7)</c>.
    /// If any field value is <see langword="null"/>, it is represented as <see cref="string.Empty"/>.
    /// </remarks>
    public override string ToString()
    {
        return "(" + Item1?.ToString() + ", " + Item2?.ToString() + ", " + Item3?.ToString() + ", " + Item4?.ToString() + ", " + Item5?.ToString() + ", " + Item6?.ToString() + ", " + Item7?.ToString() + ")";
    }
}