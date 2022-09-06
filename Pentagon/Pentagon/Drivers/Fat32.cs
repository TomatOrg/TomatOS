using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Buffers;
using System.Threading.Tasks;
using System.Diagnostics;
using Pentagon.DriverServices;
using Pentagon.Interfaces;
using System.Threading;

namespace Pentagon.Drivers
{
    class Fat32File : IFile
    {
        public string FileName => _fileName;
        public long PhysicalSize => _size;
        public long FileSize => _size;

        readonly Fat32 _fs;
        readonly uint _cluster;
        string _fileName;
        long _size;
        long _currentClusterIdx = -1;
        IMemoryOwner<byte> _currentCluster;

        internal Fat32File(Fat32 fs, uint cluster, string filename, uint size)
        {
            _fs = fs;
            _cluster = cluster;
            _fileName = filename;
            _size = size;
        }

        public async Task<int> Read(long offset, Memory<byte> buffer, CancellationToken token = default)
        {
            var cluster = offset / _fs.BlockSize;
            int index = (int)(offset % _fs.BlockSize);

            if (cluster != _currentClusterIdx) // hell
            {
                _currentClusterIdx = _cluster;
                for (int i = 0; i < cluster; i++) { _currentClusterIdx = await _fs.NextInClusterChain((uint)_currentClusterIdx); }
                _currentCluster = await _fs.ReadCached(_fs.ClusterToLba((uint)_currentClusterIdx));
            }

            int bytesRead = 0;

            while (true)
            {
                var remainingInFile = _size - offset - (long)bytesRead;
                var remainingInBuf = buffer.Length - bytesRead;
                int length = (int)Math.Min(remainingInBuf, Math.Min(remainingInFile, _fs.BlockSize));
                if (length == 0) break;
                for (int i = 0; i < length; i++)
                {
                    buffer.Span[bytesRead + i] = _currentCluster.Memory.Span[index + i];
                }
                index = 0;
                _currentClusterIdx = await _fs.NextInClusterChain((uint)_currentClusterIdx);
                _currentCluster = await _fs.ReadCached(_fs.ClusterToLba((uint)_currentClusterIdx));
                bytesRead += length;
            }
            return bytesRead;
        }

        public async Task<int> Write(long offset, Memory<byte> buffer, CancellationToken token = default)
        {
            return 0;
        }

        public async Task Delete(CancellationToken token = default) { }

        public async Task Flush(CancellationToken token = default) { }
    }
    class Fat32Directory : IDirectory
    {
        public string FileName => _fileName;
        public long PhysicalSize => _fs.BlockSize;
        public long FileSize => _fs.BlockSize;

        readonly Fat32 _fs;
        readonly uint _cluster;
        readonly string _fileName = "";

        internal Fat32Directory(Fat32 fs, uint cluster)
        {
            _fs = fs;
            _cluster = cluster;
        }

        internal Fat32Directory(Fat32 fs, uint cluster, string filename)
        {
            _fs = fs;
            _cluster = cluster;
            _fileName = filename;
        }

        public async Task<IFile> OpenFile(string filename, FileOpenMode mode, CancellationToken token = default)
        {
            Fat32File d = null;
            await _fs.Traverse(_cluster, TraversalFn);
            async Task<bool> TraversalFn(Fat32.DirentInfo f)
            {
                if (!f.IsDirectory && filename.Equals(f.Name))
                {
                    d = new(_fs, f.Cluster, f.Name, f.Size);
                    return true;
                }
                return false;
            }
            return d;
        }

        public async Task<IDirectory> OpenDirectory(string filename, FileOpenMode mode, CancellationToken token = default)
        {
            Fat32Directory d = null;
            await _fs.Traverse(_cluster, TraversalFn);
            async Task<bool> TraversalFn(Fat32.DirentInfo f)
            {
                if (f.IsDirectory && filename.Equals(f.Name))
                {
                    d = new(_fs, f.Cluster, f.Name);
                    return true;
                }
                return false;
            }
            return d;
        }

        public async Task<IFile> CreateFile(string name, DateTime creation, CancellationToken token = default)
        {
            var newCluster = await _fs.ClusterAllocate();
            await _fs.AddDirent(_cluster, name, creation, newCluster, (uint)_fs.BlockSize, false);
            return new Fat32File(_fs, newCluster, name, (uint)_fs.BlockSize);
        }

        public async Task<IDirectory> CreateDirectory(string name, DateTime creation, CancellationToken token = default)
        {
            var newCluster = await _fs.ClusterAllocate();
            // TODO: zerofill cluster to be sure
            await _fs.AddDirent(_cluster, name, creation, newCluster, 0, true);
            await _fs.AddDirent(newCluster, ".", creation, newCluster, 0, true);
            await _fs.AddDirent(newCluster, "..", creation, newCluster, 0, true);
            return new Fat32Directory(_fs, newCluster, name);
        }

        public async Task Delete(CancellationToken token = default) { }

        public async Task Flush(CancellationToken token = default) { }
    }


    class Fat32 : IFileSystem
    {
        public static Fat32 stati;
        public long BlockSize => (_block.BlockSize * _sectorsPerCluster);
        public bool ReadOnly => false;
        public long FreeSpace => 0;
        public string VolumeLabel => "";
        public long VolumeSize => 0;

        public async Task<IDirectory> OpenVolume()
        {
            return new Fat32Directory(this, _rootCluster);
        }

        IBlock _block;
        uint _rootCluster;
        public uint _sectorsPerCluster;
        uint _firstFatSector;
        uint _firstDataSector;
        uint _sectorsPerFat;
        Dictionary<long, IMemoryOwner<byte>> _cache;

        internal long ClusterToLba(uint cluster) => (cluster - 2) * _sectorsPerCluster + _firstDataSector;

        internal async Task<IMemoryOwner<byte>> ReadCached(long lba)
        {
            if (_cache.ContainsKey(lba))
            {
                return _cache[lba];
            }
            else
            {
                var mem = MemoryServices.AllocatePages(1);
                await _block.ReadBlocks(lba, mem.Memory);
                _cache[lba] = mem;
                return mem;
            }
        }

        internal async Task<uint> NextInClusterChain(uint cluster)
        {
            var byteoff = (long)cluster * 4;
            var sector = _firstFatSector + byteoff / _block.BlockSize;
            var offset = (byteoff % _block.BlockSize) / 4;
            var fatSector = MemoryMarshal.Cast<byte, uint>((await ReadCached(sector)).Memory);
            return fatSector.Span[(int)offset] & 0x0FFFFFFF;
        }

        // TODO: this can be optimized dramatically, making use of FSInfo and also doing more than 1 op at the same time
        internal async Task<uint> ClusterAllocate(uint cluster = 0xFFFFFFFF)
        {
            for (int i = 0; i < _sectorsPerFat; i++)
            {
                var fatMem = ((await ReadCached(_firstFatSector + i)).Memory);
                var fatArr = MemoryMarshal.Cast<byte, uint>(fatMem);

                uint count = (uint)_block.BlockSize / 4;
                for (int j = 0; j < count; j++)
                {
                    if ((fatArr.Span[j] & 0x0FFFFFFF) == 0)
                    {
                        // update current FAT marking it as EOC
                        fatArr.Span[j] = (fatArr.Span[j] & 0xF0000000) | 0x0FFFFFFF; // end of clusterchain
                        var newcluster = ((uint)i * count) + (uint)j;
                        await _block.WriteBlocks(_firstFatSector + i, fatMem); // update next end to be EOC

                        // update previous FAT showing the continuation
                        if (cluster != 0xFFFFFFFF)
                        {
                            var byteOff = (long)cluster * 4;
                            var sector = _firstFatSector + byteOff / _block.BlockSize;
                            if (sector != (_firstFatSector + i))
                            {
                                var offset = (byteOff % _block.BlockSize) / 4;
                                var mem = (await ReadCached(sector)).Memory;
                                var fatSector = MemoryMarshal.Cast<byte, uint>(mem);
                                // FIXME: check if it's EOC
                                fatSector.Span[(int)offset] = (fatSector.Span[(int)offset] & 0xF0000000) | newcluster;
                                await _block.WriteBlocks(sector, mem); // update current end
                            }
                        }

                        return newcluster;
                    }
                }
            }
            return 0xFFFFFFFF;
        }

        static public async Task<Fat32> CheckDevice(IBlock block)
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
            //var rootDirEnts = region.CreateField<ushort>(0x11).Value;
            var sectorsPerFat = region.CreateField<uint>(0x24).Value;
            var rootCluster = region.CreateField<uint>(0x2C).Value;

            var firstDataSector = reservedSectors + (fatCount * sectorsPerFat);

            var fat = new Fat32
            {
                _block = block,
                _rootCluster = rootCluster,
                _sectorsPerCluster = sectorsPerCluster,
                _firstFatSector = reservedSectors,
                _firstDataSector = firstDataSector,
                _sectorsPerFat = sectorsPerFat,
                _cache = new(),
            };
            stati = fat;
            return fat;
        }

        static DateTime FatToDateTime(uint dt)
        {
            var hour = (int)(dt >> 11) & 0b11111;
            var min = (int)(dt >> 5) & 0b111111;
            var sec = (int)((dt >> 0) & 0b11111) * 2; // yes, the second field stores two-second intervals
            var year = (int)((dt >> (16 + 9)) & 0b1111111) + 1980;
            var month = (int)((dt >> (16 + 5)) & 0b1111); // january is stored as month 1, but so does C# DateTime
            var day = (int)((dt >> (16 + 0)) & 0b11111);
            return new DateTime(year, month, day, hour, min, sec);
        }

        static uint DateTimeToFat(DateTime dt)
        {
            return ((uint)(dt.Year - 1980) << (16 + 9)) |
                   ((uint)dt.Month << (16 + 5)) |
                   ((uint)dt.Day << (16 + 0)) |
                   ((uint)dt.Hour << 11) |
                   ((uint)dt.Minute << 5) |
                   ((uint)(dt.Second / 2));
        }

        // TODO: replace this with an async iterator
        internal async Task Traverse(uint cluster, Func<DirentInfo, Task<bool>> callback)
        {
            var r = new Region((await ReadCached(ClusterToLba(cluster))).Memory);

            var lfnName = new char[64];
            var lfnLen = 0;

            // TODO: read whole clusterchain instead
            for (int i = 0; i < (_sectorsPerCluster * _block.BlockSize) / 32; i++)
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
                    else
                    {
                        for (; lfnLen > 0; lfnLen--) if (lfnName[lfnLen - 1] != 0xFFFF) break;
                        if (lfnName[lfnLen - 1] == 0) lfnLen--;
                    }

                    var cluste = (((uint)sfn.clusterHi.Value) << 16) | sfn.clusterLo.Value;
                    var ent = new DirentInfo
                    {
                        Name = new string(lfnName, 0, lfnLen),
                        IsDirectory = (attributes & 0x10) != 0,
                        Cluster = cluste,
                        CreatedAt = FatToDateTime(sfn.cTime.Value),
                        Size = sfn.fileSize.Value,
                    };

                    if (await callback(ent))
                    {
                        return;
                    }
                    lfnLen = 0;
                }
            }
        }

        internal async Task AddDirent(uint dirCluster, string name, DateTime creation, uint startCluster, uint size, bool isDirectory)
        {
            // atime, ctime and mtime are gonna be the same, we're creating the file now
            var fatTime = DateTimeToFat(creation);

            // how many contig entries do we need?
            var neededLfns = (name.Length + 13 - 1) / 13;
            var neededEnts = neededLfns + 1;
            var placed = 0;


            int total = (int)(_sectorsPerCluster * _block.BlockSize) / 32;

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
                dirCluster = await ClusterAllocate(dirCluster);
                r = new Region((await ReadCached(ClusterToLba(dirCluster))).Memory);

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
                    var m = (await ReadCached(ClusterToLba(dirCluster))).Memory;
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
                    dirCluster = await NextInClusterChain(dirCluster);
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
                return _block.WriteBlocks(ClusterToLba(dirCluster), r.Memory);
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
                sfn.cTime.Value = fatTime;
                sfn.aDate.Value = (ushort)(fatTime >> 16);
                sfn.mTime.Value = fatTime;
                sfn.attribs.Value = (byte)(isDirectory ? 0x10 : 0);
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

        static readonly byte[] DefaultSfn = new byte[11] { 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41 };

        static readonly byte DefaultSfnChecksum = Checksum(DefaultSfn);

        public class DirentInfo
        {
            public uint Cluster;
            public string Name;
            public bool IsDirectory;
            public DateTime CreatedAt;
            public uint Size;
        };

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
            public Field<ushort> aDate;
            public Field<uint> mTime;
            public Field<byte> attribs;

            public Sfn(Region r)
            {
                name = r.CreateMemory<byte>(0, 11);
                clusterHi = new(r, 20);
                clusterLo = new(r, 26);
                fileSize = new(r, 28);
                cTime = new(r, 14);
                aDate = new(r, 18);
                mTime = new(r, 22);
                attribs = new(r, 11);
            }
        }
    }
}
