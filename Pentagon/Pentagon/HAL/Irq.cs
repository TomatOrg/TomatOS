using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;
using System.Buffers;

namespace Pentagon.HAL
{
    public class Irq
    {
        public ulong IrqNum;

        public Irq(Memory<uint> ctrl)
        {
            IrqNum = InterruptInternal(ctrl);
        }

        public void Wait()
        {
            WaitInternal(IrqNum);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern ulong InterruptInternal(Memory<uint> ctrl);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void WaitInternal(ulong irqNum);
    };
}
