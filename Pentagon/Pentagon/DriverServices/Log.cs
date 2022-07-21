using System.Runtime.CompilerServices;

namespace Pentagon.DriverServices
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
