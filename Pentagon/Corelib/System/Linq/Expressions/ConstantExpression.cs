// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

namespace System.Linq.Expressions;

/// <summary>
/// Represents an expression that has a constant value.
/// </summary>
public class ConstantExpression : Expression
{
    internal ConstantExpression(object? value)
    {
        Value = value;
    }
    
    /// <summary>
    /// Gets the static type of the expression that this <see cref="Expression"/> represents.
    /// </summary>
    /// <returns>The <see cref="System.Type"/> that represents the static type of the expression.</returns>
    public override Type Type
    {
        get
        {
            if (Value == null)
            {
                return typeof(object);
            }

            return Value.GetType();
        }
    }
    
    /// <summary>
    /// Returns the node type of this Expression. Extension nodes should return
    /// ExpressionType.Extension when overriding this method.
    /// </summary>
    /// <returns>The <see cref="ExpressionType"/> of the expression.</returns>
    public sealed override ExpressionType NodeType => ExpressionType.Constant;

    /// <summary>
    /// Gets the value of the constant expression.
    /// </summary>
    public object? Value { get; }
    
}

public partial class Expression
{
    /// <summary>
    /// Creates a <see cref="ConstantExpression"/> that has the <see cref="ConstantExpression.Value"/> property set to the specified value. .
    /// </summary>
    /// <param name="value">An <see cref="object"/> to set the <see cref="ConstantExpression.Value"/> property equal to.</param>
    /// <returns>
    /// A <see cref="ConstantExpression"/> that has the <see cref="NodeType"/> property equal to
    /// <see cref="ExpressionType.Constant"/> and the <see cref="ConstantExpression.Value"/> property set to the specified value.
    /// </returns>
    public static ConstantExpression Constant(object? value)
    {
        return new ConstantExpression(value);
    }

}
