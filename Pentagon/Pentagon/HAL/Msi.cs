using System;

namespace Pentagon.HAL
{
    // This is the AMD64 implementation
    // future ARM64 or RISC-V support would require ifdef'ing this class
    // (assuming ARM64 == GIC, I'm not touching Pi3 or non-M1 Apple SoCs)

    static class Msi
    {
        public class MsiData
        {
            public ulong Data;
            public ulong Addr;
        };

        public static MsiData GetData(ulong irqNum, int core)
        {
            MsiData m = new()
            {
                Data = irqNum,
                Addr = 0xFEE00000ul | ((ulong)core << 12)
            };
            return m;
        }
    }
}
