using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;

namespace Pentagon.HAL
{
    internal class Log
    {
        /// <summary>
        /// printf("0x%p")
        /// </summary>
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void LogHex(ulong n);
    }
}
