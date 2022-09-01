using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Buffers;
using System.Threading.Tasks;
using System.Diagnostics;
using Pentagon.DriverServices;

namespace Pentagon.Drivers
{
    class Fat32
    {
        public static Fat32 testing;

        IBlock Block;
        uint RootCluster;
        uint SectorsPerCluster;
        uint FirstFatSector;
        uint FirstDataSector;

        Dictionary<long, IMemoryOwner<byte>> pageCache;

        long clusterToLba(uint cluster) => (cluster - 2) * SectorsPerCluster + FirstDataSector;
        async Task<IMemoryOwner<byte>> readCached(long lba)
        {
            if (pageCache.ContainsKey(lba))
            {
                return pageCache[lba];
            }
            else
            {
                var mem = MemoryServices.AllocatePages(1);
                await Block.ReadBlocks(lba, mem.Memory);
                pageCache[lba] = mem;
                return mem;
            }
        }

        async Task<uint> nextInClusterChain(uint cluster)
        {
            var byteoff = (long)cluster * 4;
            var sector = FirstFatSector + byteoff / Block.BlockSize;
            var offset = (byteoff % Block.BlockSize) / 4;
            var fatsec = MemoryMarshal.Cast<byte, uint>((await readCached(sector)).Memory);
            return fatsec.Span[(int)offset] & 0x0FFFFFFF;
        }
        static public async Task CheckDevice(IBlock block)
        {
            var mem = MemoryServices.AllocatePages(1).Memory;
            var region = new Region(mem);
            DriverServices.Log.LogString("fat32 init start\n");
            await block.ReadBlocks(0, mem);
            DriverServices.Log.LogString("read bpb\n");

            var bytesPerSector = region.CreateField<ushort>(0x0B).Value;

            if (bytesPerSector != block.BlockSize)
            {
                Log.LogString("fat32: issue\n");
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
                FirstFatSector = reservedSectors,
                FirstDataSector = firstDataSector,
                pageCache = new(),
            };

            testing = fat;
            await fat.PrintRoot();
        }


        public Task PrintRoot()
        {
            return Traverse(RootCluster, TraversalFn);

            async Task TraversalFn(File f)
            {
                Log.LogString(f.Name);
                Log.LogString(" ");
                Log.LogHex((ulong)f.CreatedAt.Year);
                Log.LogString(" ");
                Log.LogHex((ulong)f.CreatedAt.Month);
                Log.LogString(" ");
                Log.LogHex((ulong)f.CreatedAt.Day);
                Log.LogString(" ");

                Log.LogString("\n");
                if (f.IsDirectory)
                {
                    await Traverse(f.Cluster, TraversalFn);
                }
                else
                {
                    var h = (await readCached(clusterToLba(f.Cluster))).Memory;
                    Log.LogString("    ");

                    for (int i = 0; i < 16; i++)
                    {
                        Log.LogHex(h.Span[i]);
                        Log.LogString(" ");
                    }
                    Log.LogString("\n");
                    Log.LogString("    ");

                    var next = await nextInClusterChain(f.Cluster);
                    h = (await readCached(clusterToLba(next))).Memory;
                    for (int i = 0; i < 16; i++)
                    {
                        Log.LogHex(h.Span[i]);
                        Log.LogString(" ");
                    }
                    Log.LogString("\n");

                }
            }
        }
        public class File
        {
            public uint Cluster;
            public string Name;
            public bool IsDirectory;
            public DateTime CreatedAt;
        };

        static DateTime ConvertFatDate(uint datetime)
        {
            var hour = (int)(datetime >> 11) & 0b11111;
            var min = (int)(datetime >> 5) & 0b111111;
            var sec = (int)((datetime >> 0) & 0b11111) * 2; // yes, the second field stores two-second intervals
            var year = (int)((datetime >> (16 + 9)) & 0b1111111) + 1980;
            var month = (int)((datetime >> (16 + 5)) & 0b1111); // january is stored as month 1, but so does C# DateTime
            var day = (int)((datetime >> (16 + 0)) & 0b11111);
            return new DateTime(year, month, day, hour, min, sec);
        }

        // TODO: replace this with an async iterator
        public async Task Traverse(uint cluster, Func<File, Task> callback)
        {
            var r = new Region((await readCached(clusterToLba(cluster))).Memory);

            var lfnName = new char[64];
            var lfnLen = 0;

            // TODO: read whole clusterchain instead
            for (int i = 0; i < (SectorsPerCluster * Block.BlockSize) / 32; i++)
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
                    if (name.Span[0] == 0xE5) continue; // exclude deleted files
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
                            for (int j = 0; j < nameLen; j++) lfnName[j] = (char)name.Span[j];
                            lfnLen = nameLen;
                        }
                        else
                        {
                            for (int j = 0; j < nameLen; j++) lfnName[j] = (char)name.Span[j];
                            for (int j = 0; j < extLen; j++) lfnName[nameLen + 1 + j] = (char)name.Span[8 + j];
                            lfnName[nameLen] = '.';
                            lfnLen = nameLen + 1 + extLen;
                        }
                    }
                    var c = ((uint)rr.CreateField<ushort>(20).Value << 16) | rr.CreateField<ushort>(26).Value;
                    var ctime = rr.CreateField<uint>(14).Value;
                    //var atime = rr.CreateField<uint>(18).Value;
                    //var mtime = rr.CreateField<uint>(22).Value;

                    var str = new string(lfnName, 0, lfnLen);
                    var ent = new File
                    {
                        Name = str,
                        IsDirectory = (attributes & 0x10) != 0,
                        Cluster = c,
                        CreatedAt = ConvertFatDate(ctime),
                    };

                    await callback(ent);
                    lfnLen = 0;
                }
            }
        }

    }
}
