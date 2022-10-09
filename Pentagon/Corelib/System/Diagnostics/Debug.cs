// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

// Do not remove this, it is needed to retain calls to these conditional methods in release builds
#define DEBUG

using System.Diagnostics.CodeAnalysis;
using System.Runtime.CompilerServices;

namespace System.Diagnostics
{
    /// <summary>
    /// Provides a set of properties and methods for debugging code.
    /// </summary>
    public static class Debug
    {
        private static volatile DebugProvider s_provider = new DebugProvider();
        
        public static bool AutoFlush
        {
            get => true;
            set { }
        }
        
        [ThreadStatic]
        private static int t_indentLevel;
        public static int IndentLevel
        {
            get => t_indentLevel;
            set
            {
                t_indentLevel = value < 0 ? 0 : value;
                s_provider.OnIndentLevelChanged(t_indentLevel);
            }
        }

        private static volatile int s_indentSize = 4;
        public static int IndentSize
        {
            get => s_indentSize;
            set
            {
                s_indentSize = value < 0 ? 0 : value;
                s_provider.OnIndentSizeChanged(s_indentSize);
            }
        }

        [Conditional("DEBUG")]
        public static void Close() { }

        [Conditional("DEBUG")]
        public static void Flush() { }
        
        public static void Indent() =>
            IndentLevel++;

        public static void Unindent() =>
            IndentLevel--;

        public static void Print(string? message) =>
            WriteLine(message);

        public static void Print(string format, params object?[] args) =>
            WriteLine(string.Format(null, format, args));

        public static void Assert([DoesNotReturnIf(false)] bool condition) =>
            Assert(condition, string.Empty, string.Empty);
        
        public static void Assert([DoesNotReturnIf(false)] bool condition, string? message) =>
            Assert(condition, message, string.Empty);

        public static void Assert([DoesNotReturnIf(false)] bool condition, string? message, string? detailMessage)
        {
            if (!condition)
            {
                Fail(message, detailMessage);
            }
        }
        
        public static void Assert([DoesNotReturnIf(false)] bool condition, string? message, string detailMessageFormat, params object?[] args) =>
            Assert(condition, message, string.Format(detailMessageFormat, args));
        
        public static void Fail(string? message) =>
            Fail(message, string.Empty);

        public static void Fail(string? message, string? detailMessage) =>
            s_provider.Fail(message, detailMessage);

        public static void WriteLine(string? message) =>
            s_provider.WriteLine(message);

        public static void Write(string? message) =>
            s_provider.Write(message);

        public static void WriteLine(object? value) =>
            WriteLine(value?.ToString());
        
        public static void WriteLine(object? value, string? category) =>
            WriteLine(value?.ToString(), category);

        public static void WriteLine(string format, params object?[] args) =>
            WriteLine(string.Format(null, format, args));

        public static void WriteLine(string? message, string? category)
        {
            if (category == null)
            {
                WriteLine(message);
            }
            else
            {
                WriteLine(category + ": " + message);
            }
        }

        public static void Write(object? value) =>
            Write(value?.ToString());

        public static void Write(string? message, string? category)
        {
            if (category == null)
            {
                Write(message);
            }
            else
            {
                Write(category + ": " + message);
            }
        }
        
        public static void Write(object? value, string? category) =>
            Write(value?.ToString(), category);

        public static void WriteIf(bool condition, string? message)
        {
            if (condition)
            {
                Write(message);
            }
        }

        public static void WriteIf(bool condition, object? value)
        {
            if (condition)
            {
                Write(value);
            }
        }

        public static void WriteIf(bool condition, string? message, string? category)
        {
            if (condition)
            {
                Write(message, category);
            }
        }

        public static void WriteIf(bool condition, object? value, string? category)
        {
            if (condition)
            {
                Write(value, category);
            }
        }

        public static void WriteLineIf(bool condition, object? value)
        {
            if (condition)
            {
                WriteLine(value);
            }
        }

        public static void WriteLineIf(bool condition, object? value, string? category)
        {
            if (condition)
            {
                WriteLine(value, category);
            }
        }

        public static void WriteLineIf(bool condition, string? message)
        {
            if (condition)
            {
                WriteLine(message);
            }
        }

        public static void WriteLineIf(bool condition, string? message, string? category)
        {
            if (condition)
            {
                WriteLine(message, category);
            }
        }


    }

}
