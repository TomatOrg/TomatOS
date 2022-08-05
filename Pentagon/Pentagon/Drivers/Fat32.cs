using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using Pentagon.DriverServices;

namespace Pentagon.Drivers
{
    class Fat32
    {
        IBlock Block;
        uint RootCluster;
        uint SectorsPerCluster;
        uint FirstDataSector;

        static public async Task CheckDevice(IBlock block) {
            var mem = MemoryServices.AllocatePages(1).Memory;
            var region = new Region(mem);
            DriverServices.Log.LogString("fat32 init start");
            await block.ReadBlocks(0, mem);
            DriverServices.Log.LogString("read bpb");

            var bytesPerSector = region.CreateField<ushort>(0x0B).Value;
            
            if (bytesPerSector != block.BlockSize)
            {
                Log.LogString("fat32: issue");
            }

            var sectorsPerCluster = region.CreateField<byte>(0x0D).Value;
            var reservedSectors = region.CreateField<ushort>(0x0E).Value;
            var fatCount = region.CreateField<byte>(0x10).Value;
            var rootDirEnts = region.CreateField<ushort>(0x11).Value;
            var sectorsPerFat = region.CreateField<uint>(0x24).Value;
            var rootCluster = region.CreateField<uint>(0x2C).Value;
        
            var firstDataSector = reservedSectors + (fatCount * sectorsPerFat);

            var fat = new Fat32
            {
                Block = block,
                RootCluster = rootCluster,
                SectorsPerCluster = sectorsPerCluster,
                FirstDataSector = firstDataSector
            };
            await fat.Print(fat.RootCluster, 0);
        }

        public async Task Print(uint cluster, int nesting)
        {
            var sector = (cluster - 2) * SectorsPerCluster + FirstDataSector;
            var mem = MemoryServices.AllocatePages(1).Memory;
            var r = new Region(mem);
            await Block.ReadBlocks(sector, mem);
            for (int i = 0; i < 512 / 32; i++)
            {
                char[] nameStr = new char[11];
                var name = r.CreateMemory<byte>(i * 32, 11);
                var attributes = r.CreateField<byte>(i * 32 + 11).Value;
                if (attributes == (1 + 2 + 4 + 8)) continue;
                if (name.Span[0] == 0) break;
                if (name.Span[0] == '.') continue;
                for (int j = 0; j < 11; j++) nameStr[j] = (char)name.Span[j];
                Log.LogString(new string(nameStr));
                if ((attributes & 0x10) != 0)
                {
                    var childCluster = r.CreateField<byte>(i * 32 + 26).Value;
                    await Print(childCluster, nesting + 1);
                }
            }
        }

    }
}
