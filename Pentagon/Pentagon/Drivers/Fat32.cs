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

            var lfnName = new char[64];
            var lfnLen = 0;

            // TODO: read whole clusterchain instead
            for (int i = 0; i < 512 / 32; i++)
            {
                var rr = r.CreateRegion(i * 32);
                var attributes = rr.CreateField<byte>(11).Value;
                // There's an LFN. TODO: actually use the checksum to make sure that it's the right LFN.
                // DOS will ignore LFNs (by design) and may rearrange the SFN entry so it doesn't come immediately
                if (attributes == 0xF)
                {
                    var order = (rr.CreateField<byte>(0).Value & (~0x40)) - 1;
                    var start = order * 13;
                    lfnLen = Math.Max(lfnLen, start + 13);

                    var part = rr.CreateMemory<ushort>(1, 5);
                    for (int j = 0; j < part.Length; j++) lfnName[start + j] = (char)part.Span[j];

                    part = rr.CreateMemory<ushort>(14, 6);
                    for (int j = 0; j < part.Length; j++) lfnName[start + 5 + j] = (char)part.Span[j];

                    part = rr.CreateMemory<ushort>(28, 2);
                    for (int j = 0; j < part.Length; j++) lfnName[start + 11 + j] = (char)part.Span[j];
                }
                else
                {
                    var name = rr.CreateMemory<byte>(0, 11);
                    if (name.Span[0] == 0) break;
                    if (name.Span[0] == '.') continue; // exclude dot, dotdot and UNIX-like hidden entries
                    if (lfnLen == 0) // no LFN: there is only a SFN, so convert 8.3 to a normal filename 
                    {
                        // trim spaces from name
                        int nameLen = 8;
                        for (; nameLen > 0; nameLen--) if (name.Span[nameLen - 1] != ' ') break;

                        // trim spaces from extension
                        int extLen = 3;
                        for (; extLen > 0; extLen--) if (name.Span[8 + (extLen - 1)] != ' ') break;

                        if (extLen == 0)
                        {
                            var str = new char[nameLen];
                            for (int j = 0; j < nameLen; j++) str[j] = (char)name.Span[j];
                            Log.LogString(new string(str));
                        }
                        else
                        {
                            var str = new char[nameLen + 1 + extLen];
                            for (int j = 0; j < nameLen; j++) str[j] = (char)name.Span[j];
                            for (int j = 0; j < extLen; j++) str[nameLen + 1 + j] = (char)name.Span[8 + j];
                            str[nameLen] = '.';
                            Log.LogString(new string(str));
                        }

                    } else {
                        Log.LogString(new string(lfnName, 0, lfnLen));
                        lfnLen = 0;
                    }
                    if ((attributes & 0x10) != 0)
                    {
                        var childCluster = rr.CreateField<byte>(26).Value;
                        await Print(childCluster, nesting + 1);
                    }
                }
            }
        }

    }
}
