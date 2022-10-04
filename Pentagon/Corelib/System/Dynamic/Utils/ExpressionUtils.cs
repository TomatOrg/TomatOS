// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System.Linq.Expressions;

namespace System.Dynamic.Utils;

internal static class ExpressionUtils
{
    
    
    public static void RequiresCanRead(Expression expression, string paramName)
    {
        RequiresCanRead(expression, paramName, -1);
    }

    public static void RequiresCanRead(Expression expression, string paramName, int idx)
    {
        ContractUtils.RequiresNotNull(expression, paramName, idx);

        // validate that we can read the node
        switch (expression.NodeType)
        {
            // case ExpressionType.Index:
            //     IndexExpression index = (IndexExpression)expression;
            //     if (index.Indexer != null && !index.Indexer.CanRead)
            //     {
            //         throw Error.ExpressionMustBeReadable(paramName, idx);
            //     }
            //     break;
            // case ExpressionType.MemberAccess:
            //     MemberExpression member = (MemberExpression)expression;
            //     if (member.Member is PropertyInfo prop)
            //     {
            //         if (!prop.CanRead)
            //         {
            //             throw Error.ExpressionMustBeReadable(paramName, idx);
            //         }
            //     }
            //     break;
        }
    }
    
    
}