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
        uint SectorsPerFat;

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

        // TODO: this can be optimized dramatically, making use of FSInfo and also doing more than 1 op at the same time
        async Task<uint> blockAlloc(uint cluster)
        {
            for (int i = 0; i < SectorsPerFat; i++)
            {
                var m = ((await readCached(FirstFatSector + i)).Memory);

                var arr = MemoryMarshal.Cast<byte, uint>(m);
                uint count = (uint)Block.BlockSize / 4;
                for (int j = 0; j < count; j++)
                {
                    if ((arr.Span[j] & 0x0FFFFFFF) == 0)
                    {
                        arr.Span[j] = (arr.Span[j] & 0xF0000000) | 0x0FFFFFFF; // end of clusterchain
                        var newcluster = ((uint)i * count) + (uint)j;

                        var byteoff = (long)cluster * 4;
                        var sector = FirstFatSector + byteoff / Block.BlockSize;
                        var offset = (byteoff % Block.BlockSize) / 4;
                        var mem = (await readCached(sector)).Memory;
                        var fatsec = MemoryMarshal.Cast<byte, uint>(mem);
                        // FIXME: check if it's EOC
                        fatsec.Span[(int)offset] = (fatsec.Span[(int)offset] & 0xF0000000) | newcluster;
                        await Block.WriteBlocks(sector, mem); // update current end
                        await Block.WriteBlocks(FirstFatSector + i, m); // update next end to be EOC
                        return newcluster;
                    }
                }
            }
            return 0xFFFFFFFF;
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
                SectorsPerFat = sectorsPerFat,
                pageCache = new(),
            };

            testing = fat;
            await fat.PrintRoot();
            
            await fat.AddDirent(rootCluster, "ThisIsALongerName", new DateTime(), 696969, 1024);
            await fat.AddDirent(rootCluster, "ThisIsALongerName2", new DateTime(), 696969 + 1, 1024);
            await fat.AddDirent(rootCluster, "ThisIsALongerName3", new DateTime(), 696969 + 2, 1024);
            await fat.AddDirent(rootCluster, "ThisIsALongerName4", new DateTime(), 696969 + 2, 1024);
            await fat.AddDirent(rootCluster, "Short", new DateTime(), 696969 + 6, 1024);
            await fat.AddDirent(rootCluster, "Short2", new DateTime(), 696969 + 7, 1024);
            await fat.AddDirent(rootCluster, "Short3", new DateTime(), 696969 + 7, 1024);
            await fat.AddDirent(rootCluster, "Short4", new DateTime(), 696969 + 7, 1024);
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
                var attributes = rr.Span[11];

                // There's an LFN. TODO: actually use the checksum to make sure that it's the right LFN.
                // DOS will ignore LFNs (by design) and may rearrange the SFN entry so it doesn't come immediately
                if (attributes == 0xF)
                {
                    var lfn = new Lfn(rr);

                    var order = (lfn.order.Value & (~0x40)) - 1;
                    var start = order * 13;
                    lfnLen = Math.Max(lfnLen, start + 13);

                    for (int j = 0; j < lfn.part1.Length; j++) lfnName[start + j] = (char)lfn.part1.Span[j];
                    for (int j = 0; j < lfn.part2.Length; j++) lfnName[start + 5 + j] = (char)lfn.part2.Span[j];
                    for (int j = 0; j < lfn.part3.Length; j++) lfnName[start + 11 + j] = (char)lfn.part3.Span[j];
                }
                else
                {
                    var sfn = new Sfn(rr);
                    if (sfn.name.Span[0] == 0) break;
                    if (sfn.name.Span[0] == '.') continue; // exclude dot, dotdot and UNIX-like hidden entries
                    if (sfn.name.Span[0] == 0xE5) continue; // exclude deleted files
                    if (lfnLen == 0) // no LFN: there is only a SFN, so convert 8.3 to a normal filename 
                    {
                        // trim spaces from name
                        int nameLen = 8;
                        for (; nameLen > 0; nameLen--) if (sfn.name.Span[nameLen - 1] != ' ') break;

                        // trim spaces from extension
                        int extLen = 3;
                        for (; extLen > 0; extLen--) if (sfn.name.Span[8 + (extLen - 1)] != ' ') break;

                        if (extLen == 0)
                        {
                            for (int j = 0; j < nameLen; j++) lfnName[j] = (char)sfn.name.Span[j];
                            lfnLen = nameLen;
                        }
                        else
                        {
                            for (int j = 0; j < nameLen; j++) lfnName[j] = (char)sfn.name.Span[j];
                            for (int j = 0; j < extLen; j++) lfnName[nameLen + 1 + j] = (char)sfn.name.Span[8 + j];
                            lfnName[nameLen] = '.';
                            lfnLen = nameLen + 1 + extLen;
                        }
                    }

                    var cluste = (((uint)sfn.clusterHi.Value) << 16) | sfn.clusterLo.Value;
                    var ent = new File
                    {
                        Name = new string(lfnName, 0, lfnLen),
                        IsDirectory = (attributes & 0x10) != 0,
                        Cluster = cluste,
                        CreatedAt = ConvertFatDate(sfn.cTime.Value),
                    };

                    await callback(ent);
                    lfnLen = 0;
                }
            }
        }


        static byte Checksum(byte[] sfn)
        {
            byte sum = 0;
            for (int i = 0; i < 11; i++)
            {
                sum = (byte)((sum << 7) + (sum >> 1));
                sum += sfn[i];
            }
            return sum;
        }

        static byte[] DefaultSfn = new byte[11] { 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41 };
        static byte DefaultSfnChecksum = Checksum(DefaultSfn);

        class Lfn
        {
            public Field<byte> order;
            public Field<byte> attr;
            public Field<byte> type;
            public Field<byte> checksum;
            public Field<ushort> clusterLo;

            public Memory<ushort> part1;
            public Memory<ushort> part2;
            public Memory<ushort> part3;

            public Lfn(Region r)
            {
                order = new(r, 0);
                attr = new(r, 11);
                type = new(r, 12);
                checksum = new(r, 13);
                clusterLo = new(r, 26);
                part1 = r.CreateMemory<ushort>(1, 5);
                part2 = r.CreateMemory<ushort>(14, 6);
                part3 = r.CreateMemory<ushort>(28, 2);
            }
        }

        class Sfn
        {
            public Memory<byte> name;
            public Field<ushort> clusterHi;
            public Field<ushort> clusterLo;
            public Field<uint> fileSize;
            public Field<uint> cTime;

            public Sfn(Region r)
            {
                name = r.CreateMemory<byte>(0, 11);
                clusterHi = new(r, 20);
                clusterLo = new(r, 26);
                fileSize = new(r, 28);
                cTime = new(r, 14);
            }
        }
        public async Task AddDirent(uint dirCluster, string name, DateTime creation, uint startCluster, uint size)
        {
            // how many contig entries do we need?
            var neededLfns = (name.Length + 13 - 1) / 13;
            var neededEnts = neededLfns + 1;
            var placed = 0;


            int total = (int)(SectorsPerCluster * Block.BlockSize) / 32;
            
            // TODO: use tuples instead
            Region r = null;
            int i;
            await FindDirlistEnd();

            // i is the first free entry
            var remaining = total - i;
            var offset = i * 32;

            if (remaining < neededEnts)
            {
                // place all entries we can on the current block
                await PlaceNEntries(remaining);

                // update entries to write and offset
                neededEnts -= remaining;
                offset = 0;

                // allocate new block
                dirCluster = await blockAlloc(dirCluster);
                r = new Region((await readCached(clusterToLba(dirCluster))).Memory);

                // zerofill, TODO: improve
                for (int j = 0; j < MemoryServices.PageSize; j++) r.Span[j] = 0;
            }

            // if the dirent straddles two clusters, place the remaining entries in the newly-allocated one
            await PlaceNEntries(neededEnts);


            async Task FindDirlistEnd()
            {
                while (true)
                {
                    i = 0;
                    var m = (await readCached(clusterToLba(dirCluster))).Memory;
                    for (; i < total; i++)
                    {
                        // any entry with the first byte as 0x00 marks the end of the list
                        if (m.Span[i * 32] == 0x00)
                        {
                            // set the region, TODO: return a tuple instead
                            r = new(m);
                            return;
                        }
                    }
                    dirCluster = await nextInClusterChain(dirCluster);
                }
            }

            Task PlaceNEntries(int count)
            {
                for (int i = 0; i < count; i++)
                {
                    if (placed < neededLfns) PlaceLfn(placed);
                    else PlaceSfn();
                    placed++;
                }
                return Block.WriteBlocks(clusterToLba(dirCluster), r.Memory);
            }

            void PlaceLfn(int j)
            {
                var idx = neededLfns - j; // remember that it's one-indexed
                var strIdx = (idx - 1) * 13;
                var lfn = new Lfn(r.CreateRegion(offset));
                offset += 32;
                lfn.order.Value = (byte)(idx | (j == 0 ? 0x40 : 0));
                lfn.attr.Value = 0xF;
                lfn.type.Value = 0;
                lfn.checksum.Value = DefaultSfnChecksum;
                lfn.clusterLo.Value = 0;

                var entry = new ushort[13];
                for (int k = 0; k < 13; k++)
                {
                    if ((strIdx + k) < name.Length) entry[k] = name[strIdx + k];
                    else if ((strIdx + k) == name.Length) entry[k] = 0;
                    else entry[k] = 0xFFFF;
                }
                for (int k = 0; k < lfn.part1.Length; k++) lfn.part1.Span[k] = entry[k];
                for (int k = 0; k < lfn.part2.Length; k++) lfn.part2.Span[k] = entry[k + 5];
                for (int k = 0; k < lfn.part3.Length; k++) lfn.part3.Span[k] = entry[k + 11];

            }
            void PlaceSfn()
            {
                var sfn = new Sfn(r.CreateRegion(offset));
                offset += 32;
                for (int j = 0; j < 11; j++) sfn.name.Span[j] = DefaultSfn[j];
                sfn.clusterHi.Value = (ushort)(startCluster >> 16);
                sfn.clusterLo.Value = (ushort)(startCluster & 0xFFFF);
                sfn.fileSize.Value = size;
                sfn.cTime.Value = 0;
            }
        }
    }
}
