// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace System.Numerics;

public struct Vector4
{
    /// <summary>The X component of the vector.</summary>
    public float X;

    /// <summary>The Y component of the vector.</summary>
    public float Y;

    /// <summary>The Z component of the vector.</summary>
    public float Z;

    /// <summary>The W component of the vector.</summary>
    public float W;

    /// <summary>Creates a new <see cref="System.Numerics.Vector4" /> object whose four elements have the same value.</summary>
    /// <param name="value">The value to assign to all four elements.</param>
    public Vector4(float value) 
        : this(value, value, value, value)
    {
    }
    
    /// <summary>Creates a vector whose elements have the specified values.</summary>
    /// <param name="x">The value to assign to the <see cref="System.Numerics.Vector4.X" /> field.</param>
    /// <param name="y">The value to assign to the <see cref="System.Numerics.Vector4.Y" /> field.</param>
    /// <param name="z">The value to assign to the <see cref="System.Numerics.Vector4.Z" /> field.</param>
    /// <param name="w">The value to assign to the <see cref="System.Numerics.Vector4.W" /> field.</param>
    public Vector4(float x, float y, float z, float w)
    {
        X = x;
        Y = y;
        Z = z;
        W = w;
    }

    /// <summary>Constructs a vector from the given <see cref="ReadOnlySpan{Single}" />. The span must contain at least 4 elements.</summary>
    /// <param name="values">The span of elements to assign to the vector.</param>
    public Vector4(ReadOnlySpan<float> values)
    {
        if (values.Length < 4)
        {
            Vector.ThrowInsufficientNumberOfElementsException(4);
        }

        this = Unsafe.ReadUnaligned<Vector4>(ref Unsafe.As<float, byte>(ref MemoryMarshal.GetReference(values)));
    }
    
    /// <summary>Gets a vector whose 4 elements are equal to zero.</summary>
    /// <value>A vector whose four elements are equal to zero (that is, it returns the vector <c>(0,0,0,0)</c>.</value>
    public static Vector4 Zero
    {
        get => default;
    }

    /// <summary>Gets a vector whose 4 elements are equal to one.</summary>
    /// <value>Returns <see cref="System.Numerics.Vector4" />.</value>
    /// <remarks>A vector whose four elements are equal to one (that is, it returns the vector <c>(1,1,1,1)</c>.</remarks>
    public static Vector4 One
    {
        get => new Vector4(1.0f);
    }
    
    /// <summary>Gets the vector (1,0,0,0).</summary>
    /// <value>The vector <c>(1,0,0,0)</c>.</value>
    public static Vector4 UnitX
    {
        get => new Vector4(1.0f, 0.0f, 0.0f, 0.0f);
    }

    /// <summary>Gets the vector (0,1,0,0).</summary>
    /// <value>The vector <c>(0,1,0,0)</c>.</value>
    public static Vector4 UnitY
    {
        get => new Vector4(0.0f, 1.0f, 0.0f, 0.0f);
    }

    /// <summary>Gets the vector (0,0,1,0).</summary>
    /// <value>The vector <c>(0,0,1,0)</c>.</value>
    public static Vector4 UnitZ
    {
        get => new Vector4(0.0f, 0.0f, 1.0f, 0.0f);
    }

    /// <summary>Gets the vector (0,0,0,1).</summary>
    /// <value>The vector <c>(0,0,0,1)</c>.</value>
    public static Vector4 UnitW
    {
        get => new Vector4(0.0f, 0.0f, 0.0f, 1.0f);
    }
    
    /// <summary>Adds two vectors together.</summary>
    /// <param name="left">The first vector to add.</param>
    /// <param name="right">The second vector to add.</param>
    /// <returns>The summed vector.</returns>
    /// <remarks>The <see cref="System.Numerics.Vector4.op_Addition" /> method defines the addition operation for <see cref="System.Numerics.Vector4" /> objects.</remarks>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Vector4 operator +(Vector4 left, Vector4 right)
    {
        return new Vector4(
            left.X + right.X,
            left.Y + right.Y,
            left.Z + right.Z,
            left.W + right.W
        );
    }

    /// <summary>Divides the first vector by the second.</summary>
    /// <param name="left">The first vector.</param>
    /// <param name="right">The second vector.</param>
    /// <returns>The vector that results from dividing <paramref name="left" /> by <paramref name="right" />.</returns>
    /// <remarks>The <see cref="System.Numerics.Vector4.op_Division" /> method defines the division operation for <see cref="System.Numerics.Vector4" /> objects.</remarks>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Vector4 operator /(Vector4 left, Vector4 right)
    {
        return new Vector4(
            left.X / right.X,
            left.Y / right.Y,
            left.Z / right.Z,
            left.W / right.W
        );
    }

    /// <summary>Divides the specified vector by a specified scalar value.</summary>
    /// <param name="value1">The vector.</param>
    /// <param name="value2">The scalar value.</param>
    /// <returns>The result of the division.</returns>
    /// <remarks>The <see cref="System.Numerics.Vector4.op_Division" /> method defines the division operation for <see cref="System.Numerics.Vector4" /> objects.</remarks>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Vector4 operator /(Vector4 value1, float value2)
    {
        return value1 / new Vector4(value2);
    }

    /// <summary>Returns a value that indicates whether each pair of elements in two specified vectors is equal.</summary>
    /// <param name="left">The first vector to compare.</param>
    /// <param name="right">The second vector to compare.</param>
    /// <returns><see langword="true" /> if <paramref name="left" /> and <paramref name="right" /> are equal; otherwise, <see langword="false" />.</returns>
    /// <remarks>Two <see cref="System.Numerics.Vector4" /> objects are equal if each element in <paramref name="left" /> is equal to the corresponding element in <paramref name="right" />.</remarks>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static bool operator ==(Vector4 left, Vector4 right)
    {
        return (left.X == right.X)
            && (left.Y == right.Y)
            && (left.Z == right.Z)
            && (left.W == right.W);
    }

    /// <summary>Returns a value that indicates whether two specified vectors are not equal.</summary>
    /// <param name="left">The first vector to compare.</param>
    /// <param name="right">The second vector to compare.</param>
    /// <returns><see langword="true" /> if <paramref name="left" /> and <paramref name="right" /> are not equal; otherwise, <see langword="false" />.</returns>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static bool operator !=(Vector4 left, Vector4 right)
    {
        return !(left == right);
    }

    /// <summary>Returns a new vector whose values are the product of each pair of elements in two specified vectors.</summary>
    /// <param name="left">The first vector.</param>
    /// <param name="right">The second vector.</param>
    /// <returns>The element-wise product vector.</returns>
    /// <remarks>The <see cref="System.Numerics.Vector4.op_Multiply" /> method defines the multiplication operation for <see cref="System.Numerics.Vector4" /> objects.</remarks>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Vector4 operator *(Vector4 left, Vector4 right)
    {
        return new Vector4(
            left.X * right.X,
            left.Y * right.Y,
            left.Z * right.Z,
            left.W * right.W
        );
    }

    /// <summary>Multiplies the specified vector by the specified scalar value.</summary>
    /// <param name="left">The vector.</param>
    /// <param name="right">The scalar value.</param>
    /// <returns>The scaled vector.</returns>
    /// <remarks>The <see cref="System.Numerics.Vector4.op_Multiply" /> method defines the multiplication operation for <see cref="System.Numerics.Vector4" /> objects.</remarks>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Vector4 operator *(Vector4 left, float right)
    {
        return left * new Vector4(right);
    }

    /// <summary>Multiplies the scalar value by the specified vector.</summary>
    /// <param name="left">The vector.</param>
    /// <param name="right">The scalar value.</param>
    /// <returns>The scaled vector.</returns>
    /// <remarks>The <see cref="System.Numerics.Vector4.op_Multiply" /> method defines the multiplication operation for <see cref="System.Numerics.Vector4" /> objects.</remarks>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Vector4 operator *(float left, Vector4 right)
    {
        return right * left;
    }

    /// <summary>Subtracts the second vector from the first.</summary>
    /// <param name="left">The first vector.</param>
    /// <param name="right">The second vector.</param>
    /// <returns>The vector that results from subtracting <paramref name="right" /> from <paramref name="left" />.</returns>
    /// <remarks>The <see cref="System.Numerics.Vector4.op_Subtraction" /> method defines the subtraction operation for <see cref="System.Numerics.Vector4" /> objects.</remarks>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Vector4 operator -(Vector4 left, Vector4 right)
    {
        return new Vector4(
            left.X - right.X,
            left.Y - right.Y,
            left.Z - right.Z,
            left.W - right.W
        );
    }

    /// <summary>Negates the specified vector.</summary>
    /// <param name="value">The vector to negate.</param>
    /// <returns>The negated vector.</returns>
    /// <remarks>The <see cref="System.Numerics.Vector4.op_UnaryNegation" /> method defines the unary negation operation for <see cref="System.Numerics.Vector4" /> objects.</remarks>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Vector4 operator -(Vector4 value)
    {
        return Zero - value;
    }

    /// <summary>Returns a vector whose elements are the absolute values of each of the specified vector's elements.</summary>
    /// <param name="value">A vector.</param>
    /// <returns>The absolute value vector.</returns>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Vector4 Abs(Vector4 value)
    {
        return new Vector4(
            MathF.Abs(value.X),
            MathF.Abs(value.Y),
            MathF.Abs(value.Z),
            MathF.Abs(value.W)
        );
    }

    /// <summary>Adds two vectors together.</summary>
    /// <param name="left">The first vector to add.</param>
    /// <param name="right">The second vector to add.</param>
    /// <returns>The summed vector.</returns>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Vector4 Add(Vector4 left, Vector4 right)
    {
        return left + right;
    }

    /// <summary>Restricts a vector between a minimum and a maximum value.</summary>
    /// <param name="value1">The vector to restrict.</param>
    /// <param name="min">The minimum value.</param>
    /// <param name="max">The maximum value.</param>
    /// <returns>The restricted vector.</returns>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Vector4 Clamp(Vector4 value1, Vector4 min, Vector4 max)
    {
        // We must follow HLSL behavior in the case user specified min value is bigger than max value.
        return Min(Max(value1, min), max);
    }

    /// <summary>Computes the Euclidean distance between the two given points.</summary>
    /// <param name="value1">The first point.</param>
    /// <param name="value2">The second point.</param>
    /// <returns>The distance.</returns>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static float Distance(Vector4 value1, Vector4 value2)
    {
        float distanceSquared = DistanceSquared(value1, value2);
        return MathF.Sqrt(distanceSquared);
    }

    /// <summary>Returns the Euclidean distance squared between two specified points.</summary>
    /// <param name="value1">The first point.</param>
    /// <param name="value2">The second point.</param>
    /// <returns>The distance squared.</returns>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static float DistanceSquared(Vector4 value1, Vector4 value2)
    {
        Vector4 difference = value1 - value2;
        return Dot(difference, difference);
    }

    /// <summary>Divides the first vector by the second.</summary>
    /// <param name="left">The first vector.</param>
    /// <param name="right">The second vector.</param>
    /// <returns>The vector resulting from the division.</returns>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Vector4 Divide(Vector4 left, Vector4 right)
    {
        return left / right;
    }

    /// <summary>Divides the specified vector by a specified scalar value.</summary>
    /// <param name="left">The vector.</param>
    /// <param name="divisor">The scalar value.</param>
    /// <returns>The vector that results from the division.</returns>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Vector4 Divide(Vector4 left, float divisor)
    {
        return left / divisor;
    }

    /// <summary>Returns the dot product of two vectors.</summary>
    /// <param name="vector1">The first vector.</param>
    /// <param name="vector2">The second vector.</param>
    /// <returns>The dot product.</returns>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static float Dot(Vector4 vector1, Vector4 vector2)
    {
        return (vector1.X * vector2.X)
             + (vector1.Y * vector2.Y)
             + (vector1.Z * vector2.Z)
             + (vector1.W * vector2.W);
    }

    /// <summary>Performs a linear interpolation between two vectors based on the given weighting.</summary>
    /// <param name="value1">The first vector.</param>
    /// <param name="value2">The second vector.</param>
    /// <param name="amount">A value between 0 and 1 that indicates the weight of <paramref name="value2" />.</param>
    /// <returns>The interpolated vector.</returns>
    /// <remarks><format type="text/markdown"><![CDATA[
    /// The behavior of this method changed in .NET 5.0. For more information, see [Behavior change for Vector2.Lerp and Vector4.Lerp](/dotnet/core/compatibility/3.1-5.0#behavior-change-for-vector2lerp-and-vector4lerp).
    /// ]]></format></remarks>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Vector4 Lerp(Vector4 value1, Vector4 value2, float amount)
    {
        return (value1 * (1.0f - amount)) + (value2 * amount);
    }

    /// <summary>Returns a vector whose elements are the maximum of each of the pairs of elements in two specified vectors.</summary>
    /// <param name="value1">The first vector.</param>
    /// <param name="value2">The second vector.</param>
    /// <returns>The maximized vector.</returns>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Vector4 Max(Vector4 value1, Vector4 value2)
    {
        return new Vector4(
            (value1.X > value2.X) ? value1.X : value2.X,
            (value1.Y > value2.Y) ? value1.Y : value2.Y,
            (value1.Z > value2.Z) ? value1.Z : value2.Z,
            (value1.W > value2.W) ? value1.W : value2.W
        );
    }

    /// <summary>Returns a vector whose elements are the minimum of each of the pairs of elements in two specified vectors.</summary>
    /// <param name="value1">The first vector.</param>
    /// <param name="value2">The second vector.</param>
    /// <returns>The minimized vector.</returns>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Vector4 Min(Vector4 value1, Vector4 value2)
    {
        return new Vector4(
            (value1.X < value2.X) ? value1.X : value2.X,
            (value1.Y < value2.Y) ? value1.Y : value2.Y,
            (value1.Z < value2.Z) ? value1.Z : value2.Z,
            (value1.W < value2.W) ? value1.W : value2.W
        );
    }

    /// <summary>Returns a new vector whose values are the product of each pair of elements in two specified vectors.</summary>
    /// <param name="left">The first vector.</param>
    /// <param name="right">The second vector.</param>
    /// <returns>The element-wise product vector.</returns>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Vector4 Multiply(Vector4 left, Vector4 right)
    {
        return left * right;
    }

    /// <summary>Multiplies a vector by a specified scalar.</summary>
    /// <param name="left">The vector to multiply.</param>
    /// <param name="right">The scalar value.</param>
    /// <returns>The scaled vector.</returns>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Vector4 Multiply(Vector4 left, float right)
    {
        return left * right;
    }

    /// <summary>Multiplies a scalar value by a specified vector.</summary>
    /// <param name="left">The scaled value.</param>
    /// <param name="right">The vector.</param>
    /// <returns>The scaled vector.</returns>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Vector4 Multiply(float left, Vector4 right)
    {
        return left * right;
    }

    /// <summary>Negates a specified vector.</summary>
    /// <param name="value">The vector to negate.</param>
    /// <returns>The negated vector.</returns>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Vector4 Negate(Vector4 value)
    {
        return -value;
    }

    /// <summary>Returns a vector with the same direction as the specified vector, but with a length of one.</summary>
    /// <param name="vector">The vector to normalize.</param>
    /// <returns>The normalized vector.</returns>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Vector4 Normalize(Vector4 vector)
    {
        return vector / vector.Length();
    }

    /// <summary>Returns a vector whose elements are the square root of each of a specified vector's elements.</summary>
    /// <param name="value">A vector.</param>
    /// <returns>The square root vector.</returns>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Vector4 SquareRoot(Vector4 value)
    {
        return new Vector4(
            MathF.Sqrt(value.X),
            MathF.Sqrt(value.Y),
            MathF.Sqrt(value.Z),
            MathF.Sqrt(value.W)
        );
    }

    /// <summary>Subtracts the second vector from the first.</summary>
    /// <param name="left">The first vector.</param>
    /// <param name="right">The second vector.</param>
    /// <returns>The difference vector.</returns>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Vector4 Subtract(Vector4 left, Vector4 right)
    {
        return left - right;
    }

    /// <summary>Returns the length of this vector object.</summary>
    /// <returns>The vector's length.</returns>
    /// <altmember cref="System.Numerics.Vector4.LengthSquared"/>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public readonly float Length()
    {
        float lengthSquared = LengthSquared();
        return MathF.Sqrt(lengthSquared);
    }

    /// <summary>Returns the length of the vector squared.</summary>
    /// <returns>The vector's length squared.</returns>
    /// <remarks>This operation offers better performance than a call to the <see cref="System.Numerics.Vector4.Length" /> method.</remarks>
    /// <altmember cref="System.Numerics.Vector4.Length"/>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public readonly float LengthSquared()
    {
        return Dot(this, this);
    }

}