// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System.Collections;
using System.Diagnostics;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Threading;

namespace System
{
    public static class Environment
    {
        
        
        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Native)]
        private static extern int GetProcessorCount();

        public static int ProcessorCount { get; } = GetProcessorCount();

        /// <summary>
        /// Gets whether the current machine has only a single processor.
        /// </summary>
        internal static bool IsSingleProcessor => ProcessorCount == 1;

        // Unconditionally return false since .NET Core does not support object finalization during shutdown.
        public static bool HasShutdownStarted => false;

        public static bool Is64BitProcess => true;

        public static bool Is64BitOperatingSystem => true;

        public static int TickCount => (int)TickCount64;

        public static long TickCount64 => new TimeSpan(Stopwatch.GetTimestamp()).Milliseconds;

        // TODO: this is very wrong lmao, we don't really have the concept of a thread id right now 
        public static int CurrentManagedThreadId => Thread.CurrentThread.GetHashCode();
        
        public static string NewLine => "\n";

        internal const string NewLineConst = "\n";

    }
}
