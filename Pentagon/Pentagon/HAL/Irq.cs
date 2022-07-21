using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;
using System.Buffers;

namespace Pentagon.HAL
{
    internal class Irq
    {
        public ulong IrqNum;

        internal Irq(Memory<uint> ctrl)
        {
            IrqNum = InterruptInternal(ctrl);
        }

        internal void Wait()
        {
            WaitInternal(IrqNum);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern ulong InterruptInternal(Memory<uint> ctrl);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void WaitInternal(ulong irqNum);
    };
}
