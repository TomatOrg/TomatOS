// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System.Diagnostics.CodeAnalysis;
using System.Runtime.CompilerServices;

namespace System.Diagnostics;

/// <summary>
/// Provides default implementation for Write and Fail methods in Debug class.
/// </summary>
public class DebugProvider
{

    [DoesNotReturn]
    public virtual void Fail(string? message, string? detailMessage)
    {
        var stackTrace = "";
        WriteAssert(stackTrace, message, detailMessage);
        throw new DebugAssertException(message, detailMessage, stackTrace);
    }
    
    internal void WriteAssert(string stackTrace, string? message, string? detailMessage)
    {
        WriteLine("---- DEBUG ASSERTION FAILED ----" + Environment.NewLineConst
                       + "---- Assert Short Message ----" + Environment.NewLineConst
                       + message + Environment.NewLineConst
                       + "---- Assert Long Message ----" + Environment.NewLineConst
                       + detailMessage + Environment.NewLineConst
                       + stackTrace);
    }

    public virtual void Write(string? message)
    {
        lock (s_lock)
        {
            if (message == null)
            {
                WriteInternal(string.Empty);
                return;
            }
            if (_needIndent)
            {
                message = GetIndentString() + message;
                _needIndent = false;
            }
            WriteInternal(message);
            if (message.EndsWith(Environment.NewLineConst, StringComparison.Ordinal))
            {
                _needIndent = true;
            }
        }
    }

    public virtual void WriteLine(string? message)
    {
        Write(message + Environment.NewLineConst);
    }

    public virtual void OnIndentLevelChanged(int indentLevel) { }

    public virtual void OnIndentSizeChanged(int indentSize) { }

    private static readonly object s_lock = new object();
    
    private sealed class DebugAssertException : Exception
    {
        internal DebugAssertException(string? message, string? detailMessage, string? stackTrace) :
            base(Terminate(message) + Terminate(detailMessage) + stackTrace)
        {
        }

        private static string? Terminate(string? s)
        {
            if (s == null)
                return s;

            s = s.Trim();
            if (s.Length > 0)
                s += Environment.NewLineConst;

            return s;
        }
    }

    private bool _needIndent = true;

    private string? _indentString;

    private string GetIndentString()
    {
        int indentCount = Debug.IndentSize * Debug.IndentLevel;
        if (_indentString?.Length == indentCount)
        {
            return _indentString;
        }

        // TODO: ctor that takes a char and count and repeats it count times
        return _indentString;
        // return _indentString = new string(' ', indentCount);
    }

    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Native)]
    private static extern void WriteInternal(string? message);

    
}