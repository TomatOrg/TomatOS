// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System.Collections.Generic;
using System.Reflection;

namespace System.Linq.Expressions;

/// <summary>
///    Strongly-typed and parameterized exception factory.
/// </summary>
internal static class Error
{
    /// <summary>
    /// ArgumentException with message like "reducible nodes must override Expression.Reduce()"
    /// </summary>
    internal static Exception ReducibleMustOverrideReduce()
    {
        return new ArgumentException("reducible nodes must override Expression.Reduce()");
    }
    
    /// <summary>
    /// InvalidOperationException with message like "The binary operator {0} is not defined for the types '{1}' and '{2}'."
    /// </summary>
    internal static Exception BinaryOperatorNotDefined(object? p0, object? p1, object? p2)
    {
        return new InvalidOperationException($"The binary operator {p0} is not defined for the types '{p1}' and '{p2}'.");
    }
    
    /// <summary>
    /// ArgumentException with message like "Unhandled binary: {0}"
    /// </summary>
    internal static Exception UnhandledBinary(object? p0, string? paramName)
    {
        return new ArgumentException($"Unhandled binary: {p0}", paramName);
    }
    
    /// <summary>
    /// InvalidOperationException with message like "Extension node must override the property {0}."
    /// </summary>
    internal static Exception ExtensionNodeMustOverrideProperty(object? p0)
    {
        return new InvalidOperationException($"Extension node must override the property {p0}.");
    }
    
    /// <summary>
    /// ArgumentException with message like "Argument type cannot be System.Void."
    /// </summary>
    internal static Exception ArgumentCannotBeOfTypeVoid(string? paramName)
    {
        return new ArgumentException("Argument type cannot be System.Void.", paramName);
    }

    /// <summary>
    /// ArgumentException with message like "Type must not be ByRef"
    /// </summary>
    internal static Exception TypeMustNotBeByRef(string? paramName)
    {
        return new ArgumentException("Type must not be ByRef", paramName);
    }

    /// <summary>
    /// ArgumentException with message like "Type must not be a pointer type"
    /// </summary>
    internal static Exception TypeMustNotBePointer(string? paramName)
    {
        return new ArgumentException("Type must not be a pointer type", paramName);
    }

    /// <summary>
    /// ArgumentException with message like "Type {0} contains generic parameters"
    /// </summary>
    private static Exception TypeContainsGenericParameters(object? p0, string? paramName)
    {
        return new ArgumentException($"Type {p0} contains generic parameters", paramName);
    }

    /// <summary>
    /// ArgumentException with message like "Type {0} contains generic parameters"
    /// </summary>
    internal static Exception TypeContainsGenericParameters(object? p0, string? paramName, int index)
    {
        return TypeContainsGenericParameters(p0, GetParamName(paramName, index));
    }

    /// <summary>
    /// ArgumentException with message like "Type {0} is a generic type definition"
    /// </summary>
    internal static Exception TypeIsGeneric(object? p0, string? paramName)
    {
        return new ArgumentException($"Type {p0} is a generic type definition", paramName);
    }
    
    /// <summary>
    /// ArgumentException with message like "Type {0} is a generic type definition"
    /// </summary>
    internal static Exception TypeIsGeneric(object? p0, string? paramName, int index)
    {
        return TypeIsGeneric(p0, GetParamName(paramName, index));
    }

    internal static Exception InvalidArgumentValue(string? paramName)
    {
        return new ArgumentException("Invalid argument value", paramName);
    }
    
    internal static Exception NonEmptyCollectionRequired(string? paramName)
    {
        return new ArgumentException("Non-empty collection required", paramName);
    }
    
    private static string? GetParamName(string? paramName, int index)
    {
        if (index >= 0)
        {
            return $"{paramName}[{index}]";
        }

        return paramName;
    }
}