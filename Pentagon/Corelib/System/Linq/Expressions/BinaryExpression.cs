// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System.Dynamic.Utils;
using System.Reflection;

namespace System.Linq.Expressions;

/// <summary>
/// Represents an expression that has a binary operator.
/// </summary>
public abstract class BinaryExpression : Expression
{
    
    internal BinaryExpression(Expression left, Expression right)
    {
        Left = left;
        Right = right;
    }
    
    /// <summary>
    /// Gets the right operand of the binary operation.
    /// </summary>
    public Expression Right { get; }

    /// <summary>
    /// Gets the left operand of the binary operation.
    /// </summary>
    public Expression Left { get; }

}

// Optimized representation of simple logical expressions:
// && || == != > < >= <=
internal sealed class LogicalBinaryExpression : BinaryExpression
{
    internal LogicalBinaryExpression(ExpressionType nodeType, Expression left, Expression right)
        : base(left, right)
    {
        NodeType = nodeType;
    }

    public sealed override Type Type => typeof(bool);

    public sealed override ExpressionType NodeType { get; }
}

// Optimized assignment node, only holds onto children
internal class AssignBinaryExpression : BinaryExpression
{
    internal AssignBinaryExpression(Expression left, Expression right)
        : base(left, right)
    {
    }

    public static AssignBinaryExpression Make(Expression left, Expression right, bool byRef)
    {
        if (byRef)
        {
            return new ByRefAssignBinaryExpression(left, right);
        }
        else
        {
            return new AssignBinaryExpression(left, right);
        }
    }

    internal virtual bool IsByRef => false;

    public sealed override Type Type => Left.Type;

    public sealed override ExpressionType NodeType => ExpressionType.Assign;
}

internal sealed class ByRefAssignBinaryExpression : AssignBinaryExpression
{
    internal ByRefAssignBinaryExpression(Expression left, Expression right)
        : base(left, right)
    {
    }

    internal override bool IsByRef => true;
}

// Class that handles most binary expressions
// If needed, it can be optimized even more (often Type == left.Type)
internal class SimpleBinaryExpression : BinaryExpression
{
    internal SimpleBinaryExpression(ExpressionType nodeType, Expression left, Expression right, Type type)
        : base(left, right)
    {
        NodeType = nodeType;
        Type = type;
    }

    public sealed override ExpressionType NodeType { get; }

    public sealed override Type Type { get; }
}


public partial class Expression
{
    
    
    private static BinaryExpression GetUserDefinedBinaryOperatorOrThrow(ExpressionType binaryType, string name, Expression left, Expression right, bool liftToNull)
    {
        // BinaryExpression? b = GetUserDefinedBinaryOperator(binaryType, name, left, right, liftToNull);
        // if (b != null)
        // {
        //     ParameterInfo[] pis = b.Method!.GetParametersCached();
        //     ValidateParamswithOperandsOrThrow(pis[0].ParameterType, left.Type, binaryType, name);
        //     ValidateParamswithOperandsOrThrow(pis[1].ParameterType, right.Type, binaryType, name);
        //     return b;
        // }
        throw Error.BinaryOperatorNotDefined(binaryType, left.Type, right.Type);
    }


    ///
    /// <summary>
    /// Creates a <see cref="BinaryExpression" />, given the left and right operands, by calling an appropriate factory method.
    /// </summary>
    /// <param name="binaryType">The ExpressionType that specifies the type of binary operation.</param>
    /// <param name="left">An Expression that represents the left operand.</param>
    /// <param name="right">An Expression that represents the right operand.</param>
    /// <param name="liftToNull">true to set IsLiftedToNull to true; false to set IsLiftedToNull to false.</param>
    /// <param name="method">A MethodInfo that specifies the implementing method.</param>
    /// <param name="conversion">A LambdaExpression that represents a type conversion function. This parameter is used if binaryType is Coalesce or compound assignment.</param>
    /// <returns>The BinaryExpression that results from calling the appropriate factory method.</returns>
    public static BinaryExpression MakeBinary(ExpressionType binaryType, Expression left, Expression right, bool liftToNull, MethodInfo? method/*, LambdaExpression? conversion*/) =>
        binaryType switch
        {
            ExpressionType.Add => Add(left, right, method),
            // ExpressionType.AddChecked => AddChecked(left, right, method),
            ExpressionType.Subtract => Subtract(left, right, method),
            // ExpressionType.SubtractChecked => SubtractChecked(left, right, method),
            ExpressionType.Multiply => Multiply(left, right, method),
            // ExpressionType.MultiplyChecked => MultiplyChecked(left, right, method),
            ExpressionType.Divide => Divide(left, right, method),
            ExpressionType.Modulo => Modulo(left, right, method),
            // ExpressionType.Power => Power(left, right, method),
            // ExpressionType.And => And(left, right, method),
            // ExpressionType.AndAlso => AndAlso(left, right, method),
            // ExpressionType.Or => Or(left, right, method),
            // ExpressionType.OrElse => OrElse(left, right, method),
            // ExpressionType.LessThan => LessThan(left, right, liftToNull, method),
            // ExpressionType.LessThanOrEqual => LessThanOrEqual(left, right, liftToNull, method),
            // ExpressionType.GreaterThan => GreaterThan(left, right, liftToNull, method),
            // ExpressionType.GreaterThanOrEqual => GreaterThanOrEqual(left, right, liftToNull, method),
            // ExpressionType.Equal => Equal(left, right, liftToNull, method),
            // ExpressionType.NotEqual => NotEqual(left, right, liftToNull, method),
            // ExpressionType.ExclusiveOr => ExclusiveOr(left, right, method),
            // ExpressionType.Coalesce => Coalesce(left, right, conversion),
            // ExpressionType.ArrayIndex => ArrayIndex(left, right),
            // ExpressionType.RightShift => RightShift(left, right, method),
            // ExpressionType.LeftShift => LeftShift(left, right, method),
            // ExpressionType.Assign => Assign(left, right),
            // ExpressionType.AddAssign => AddAssign(left, right, method, conversion),
            // ExpressionType.AndAssign => AndAssign(left, right, method, conversion),
            // ExpressionType.DivideAssign => DivideAssign(left, right, method, conversion),
            // ExpressionType.ExclusiveOrAssign => ExclusiveOrAssign(left, right, method, conversion),
            // ExpressionType.LeftShiftAssign => LeftShiftAssign(left, right, method, conversion),
            // ExpressionType.ModuloAssign => ModuloAssign(left, right, method, conversion),
            // ExpressionType.MultiplyAssign => MultiplyAssign(left, right, method, conversion),
            // ExpressionType.OrAssign => OrAssign(left, right, method, conversion),
            // ExpressionType.PowerAssign => PowerAssign(left, right, method, conversion),
            // ExpressionType.RightShiftAssign => RightShiftAssign(left, right, method, conversion),
            // ExpressionType.SubtractAssign => SubtractAssign(left, right, method, conversion),
            // ExpressionType.AddAssignChecked => AddAssignChecked(left, right, method, conversion),
            // ExpressionType.SubtractAssignChecked => SubtractAssignChecked(left, right, method, conversion),
            // ExpressionType.MultiplyAssignChecked => MultiplyAssignChecked(left, right, method, conversion),
            _ => throw Error.UnhandledBinary(binaryType, nameof(binaryType)),
        };

    #region Arithmetic Expressions

    /// <summary>
    /// Creates a <see cref="BinaryExpression"/> that represents an arithmetic addition operation that does not have overflow checking.
    /// </summary>
    /// <param name="left">An <see cref="Expression"/> to set the <see cref="BinaryExpression.Left"/> property equal to.</param>
    /// <param name="right">An <see cref="Expression"/> to set the <see cref="BinaryExpression.Right"/> property equal to.</param>
    /// <returns>A <see cref="BinaryExpression"/> that has the <see cref="NodeType"/> property equal to <see cref="ExpressionType.Add"/>
    /// and the <see cref="BinaryExpression.Left"/> and <see cref="BinaryExpression.Right"/> properties set to the specified values.</returns>
    public static BinaryExpression Add(Expression left, Expression right)
    {
        return Add(left, right, method: null);
    }

    /// <summary>
    /// Creates a <see cref="BinaryExpression"/> that represents an arithmetic addition operation that does not have overflow checking.
    /// </summary>
    /// <param name="left">An <see cref="Expression"/> to set the <see cref="BinaryExpression.Left"/> property equal to.</param>
    /// <param name="right">An <see cref="Expression"/> to set the <see cref="BinaryExpression.Right"/> property equal to.</param>
    /// <param name="method">A <see cref="MethodInfo"/> to set the <see cref="BinaryExpression.Method"/> property equal to.</param>
    /// <returns>A <see cref="BinaryExpression"/> that has the <see cref="NodeType"/> property equal to <see cref="ExpressionType.Add"/>
    /// and the <see cref="BinaryExpression.Left"/>, <see cref="BinaryExpression.Right"/>, and <see cref="BinaryExpression.Method"/> properties set to the specified values.
    /// </returns>
    public static BinaryExpression Add(Expression left, Expression right, MethodInfo? method)
    {
        ExpressionUtils.RequiresCanRead(left, nameof(left));
        ExpressionUtils.RequiresCanRead(right, nameof(right));
        if (method == null)
        {
            if (left.Type == right.Type && left.Type.IsArithmetic())
            {
                return new SimpleBinaryExpression(ExpressionType.Add, left, right, left.Type);
            }
            return GetUserDefinedBinaryOperatorOrThrow(ExpressionType.Add, "op_Addition", left, right, liftToNull: true);
        }
        throw new NotImplementedException();
        // return GetMethodBasedBinaryOperator(ExpressionType.Add, left, right, method, liftToNull: true);
    }
    
    /// <summary>
    /// Creates a <see cref="BinaryExpression"/> that represents an arithmetic subtraction operation that does not have overflow checking.
    /// </summary>
    /// <param name="left">An <see cref="Expression"/> to set the <see cref="BinaryExpression.Left"/> property equal to.</param>
    /// <param name="right">An <see cref="Expression"/> to set the <see cref="BinaryExpression.Right"/> property equal to.</param>
    /// <returns>A <see cref="BinaryExpression"/> that has the <see cref="NodeType"/> property equal to <see cref="ExpressionType.Subtract"/>
    /// and the <see cref="BinaryExpression.Left"/> and <see cref="BinaryExpression.Right"/> properties set to the specified values.</returns>
    public static BinaryExpression Subtract(Expression left, Expression right)
    {
        return Subtract(left, right, method: null);
    }

    /// <summary>
    /// Creates a <see cref="BinaryExpression"/> that represents an arithmetic subtraction operation that does not have overflow checking.
    /// </summary>
    /// <param name="left">An <see cref="Expression"/> to set the <see cref="BinaryExpression.Left"/> property equal to.</param>
    /// <param name="right">An <see cref="Expression"/> to set the <see cref="BinaryExpression.Right"/> property equal to.</param>
    /// <param name="method">A <see cref="MethodInfo"/> to set the <see cref="BinaryExpression.Method"/> property equal to.</param>
    /// <returns>A <see cref="BinaryExpression"/> that has the <see cref="NodeType"/> property equal to <see cref="ExpressionType.Subtract"/>
    /// and the <see cref="BinaryExpression.Left"/>, <see cref="BinaryExpression.Right"/>, and <see cref="BinaryExpression.Method"/> properties set to the specified values.
    /// </returns>
    public static BinaryExpression Subtract(Expression left, Expression right, MethodInfo? method)
    {
        ExpressionUtils.RequiresCanRead(left, nameof(left));
        ExpressionUtils.RequiresCanRead(right, nameof(right));
        if (method == null)
        {
            if (left.Type == right.Type && left.Type.IsArithmetic())
            {
                return new SimpleBinaryExpression(ExpressionType.Subtract, left, right, left.Type);
            }
            return GetUserDefinedBinaryOperatorOrThrow(ExpressionType.Subtract, "op_Subtraction", left, right, liftToNull: true);
        }
        throw new NotImplementedException();
        // return GetMethodBasedBinaryOperator(ExpressionType.Subtract, left, right, method, liftToNull: true);
    }
    
    /// <summary>
    /// Creates a <see cref="BinaryExpression"/> that represents an arithmetic division operation.
    /// </summary>
    /// <param name="left">An <see cref="Expression"/> to set the <see cref="BinaryExpression.Left"/> property equal to.</param>
    /// <param name="right">An <see cref="Expression"/> to set the <see cref="BinaryExpression.Right"/> property equal to.</param>
    /// <returns>A <see cref="BinaryExpression"/> that has the <see cref="NodeType"/> property equal to <see cref="ExpressionType.Divide"/>
    /// and the <see cref="BinaryExpression.Left"/> and <see cref="BinaryExpression.Right"/> properties set to the specified values.</returns>
    public static BinaryExpression Divide(Expression left, Expression right)
    {
        return Divide(left, right, method: null);
    }

    /// <summary>
    /// Creates a <see cref="BinaryExpression"/> that represents an arithmetic division operation.
    /// </summary>
    /// <param name="left">An <see cref="Expression"/> to set the <see cref="BinaryExpression.Left"/> property equal to.</param>
    /// <param name="right">An <see cref="Expression"/> to set the <see cref="BinaryExpression.Right"/> property equal to.</param>
    /// <param name="method">A <see cref="MethodInfo"/> to set the <see cref="BinaryExpression.Method"/> property equal to.</param>
    /// <returns>A <see cref="BinaryExpression"/> that has the <see cref="NodeType"/> property equal to <see cref="ExpressionType.Divide"/>
    /// and the <see cref="BinaryExpression.Left"/>, <see cref="BinaryExpression.Right"/>, and <see cref="BinaryExpression.Method"/> properties set to the specified values.
    /// </returns>
    public static BinaryExpression Divide(Expression left, Expression right, MethodInfo? method)
    {
        ExpressionUtils.RequiresCanRead(left, nameof(left));
        ExpressionUtils.RequiresCanRead(right, nameof(right));
        if (method == null)
        {
            if (left.Type == right.Type && left.Type.IsArithmetic())
            {
                return new SimpleBinaryExpression(ExpressionType.Divide, left, right, left.Type);
            }
            return GetUserDefinedBinaryOperatorOrThrow(ExpressionType.Divide, "op_Division", left, right, liftToNull: true);
        }
        throw new NotImplementedException();
        // return GetMethodBasedBinaryOperator(ExpressionType.Divide, left, right, method, liftToNull: true);
    }

    /// <summary>
    /// Creates a <see cref="BinaryExpression"/> that represents an arithmetic remainder operation.
    /// </summary>
    /// <param name="left">An <see cref="Expression"/> to set the <see cref="BinaryExpression.Left"/> property equal to.</param>
    /// <param name="right">An <see cref="Expression"/> to set the <see cref="BinaryExpression.Right"/> property equal to.</param>
    /// <returns>A <see cref="BinaryExpression"/> that has the <see cref="NodeType"/> property equal to <see cref="ExpressionType.Modulo"/>
    /// and the <see cref="BinaryExpression.Left"/> and <see cref="BinaryExpression.Right"/> properties set to the specified values.</returns>
    public static BinaryExpression Modulo(Expression left, Expression right)
    {
        return Modulo(left, right, method: null);
    }

    /// <summary>
    /// Creates a <see cref="BinaryExpression"/> that represents an arithmetic remainder operation.
    /// </summary>
    /// <param name="left">An <see cref="Expression"/> to set the <see cref="BinaryExpression.Left"/> property equal to.</param>
    /// <param name="right">An <see cref="Expression"/> to set the <see cref="BinaryExpression.Right"/> property equal to.</param>
    /// <param name="method">A <see cref="MethodInfo"/> to set the <see cref="BinaryExpression.Method"/> property equal to.</param>
    /// <returns>A <see cref="BinaryExpression"/> that has the <see cref="NodeType"/> property equal to <see cref="ExpressionType.Modulo"/>
    /// and the <see cref="BinaryExpression.Left"/>, <see cref="BinaryExpression.Right"/>, and <see cref="BinaryExpression.Method"/> properties set to the specified values.
    /// </returns>
    public static BinaryExpression Modulo(Expression left, Expression right, MethodInfo? method)
    {
        ExpressionUtils.RequiresCanRead(left, nameof(left));
        ExpressionUtils.RequiresCanRead(right, nameof(right));
        if (method == null)
        {
            if (left.Type == right.Type && left.Type.IsArithmetic())
            {
                return new SimpleBinaryExpression(ExpressionType.Modulo, left, right, left.Type);
            }
            return GetUserDefinedBinaryOperatorOrThrow(ExpressionType.Modulo, "op_Modulus", left, right, liftToNull: true);
        }
        throw new NotImplementedException();
        // return GetMethodBasedBinaryOperator(ExpressionType.Modulo, left, right, method, liftToNull: true);
    }
    
    /// <summary>
    /// Creates a <see cref="BinaryExpression"/> that represents an arithmetic multiplication operation that does not have overflow checking.
    /// </summary>
    /// <param name="left">An <see cref="Expression"/> to set the <see cref="BinaryExpression.Left"/> property equal to.</param>
    /// <param name="right">An <see cref="Expression"/> to set the <see cref="BinaryExpression.Right"/> property equal to.</param>
    /// <returns>A <see cref="BinaryExpression"/> that has the <see cref="NodeType"/> property equal to <see cref="ExpressionType.Multiply"/>
    /// and the <see cref="BinaryExpression.Left"/> and <see cref="BinaryExpression.Right"/> properties set to the specified values.</returns>
    public static BinaryExpression Multiply(Expression left, Expression right)
    {
        return Multiply(left, right, method: null);
    }

    /// <summary>
    /// Creates a <see cref="BinaryExpression"/> that represents an arithmetic multiplication operation that does not have overflow checking.
    /// </summary>
    /// <param name="left">An <see cref="Expression"/> to set the <see cref="BinaryExpression.Left"/> property equal to.</param>
    /// <param name="right">An <see cref="Expression"/> to set the <see cref="BinaryExpression.Right"/> property equal to.</param>
    /// <param name="method">A <see cref="MethodInfo"/> to set the <see cref="BinaryExpression.Method"/> property equal to.</param>
    /// <returns>A <see cref="BinaryExpression"/> that has the <see cref="NodeType"/> property equal to <see cref="ExpressionType.Multiply"/>
    /// and the <see cref="BinaryExpression.Left"/>, <see cref="BinaryExpression.Right"/>, and <see cref="BinaryExpression.Method"/> properties set to the specified values.
    /// </returns>
    public static BinaryExpression Multiply(Expression left, Expression right, MethodInfo? method)
    {
        ExpressionUtils.RequiresCanRead(left, nameof(left));
        ExpressionUtils.RequiresCanRead(right, nameof(right));
        if (method == null)
        {
            if (left.Type == right.Type && left.Type.IsArithmetic())
            {
                return new SimpleBinaryExpression(ExpressionType.Multiply, left, right, left.Type);
            }
            return GetUserDefinedBinaryOperatorOrThrow(ExpressionType.Multiply, "op_Multiply", left, right, liftToNull: true);
        }
        throw new NotImplementedException();
        // return GetMethodBasedBinaryOperator(ExpressionType.Multiply, left, right, method, liftToNull: true);
    }
    
    #endregion
    
}
