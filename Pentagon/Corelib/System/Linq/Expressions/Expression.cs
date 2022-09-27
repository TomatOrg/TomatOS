// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

namespace System.Linq.Expressions;

/// <summary>
/// The base type for all nodes in Expression Trees.
/// </summary>
public abstract partial class Expression
{

    
    /// <summary>
    /// Constructs a new instance of <see cref="Expression"/>.
    /// </summary>
    protected Expression()
    {
    }

    /// <summary>
    /// The <see cref="ExpressionType"/> of the <see cref="Expression"/>.
    /// </summary>
    public abstract ExpressionType NodeType { get; }


    /// <summary>
    /// The <see cref="Type"/> of the value represented by this <see cref="Expression"/>.
    /// </summary>
    public abstract Type Type { get; }
    
    /// <summary>
    /// Indicates that the node can be reduced to a simpler node. If this
    /// returns true, Reduce() can be called to produce the reduced form.
    /// </summary>
    public virtual bool CanReduce => false;

    /// <summary>
    /// Reduces this node to a simpler expression. If CanReduce returns
    /// true, this should return a valid expression. This method is
    /// allowed to return another node which itself must be reduced.
    /// </summary>
    /// <returns>The reduced expression.</returns>
    public virtual Expression Reduce()
    {
        if (CanReduce) throw Error.ReducibleMustOverrideReduce();
        return this;
    }
}