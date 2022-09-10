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
    class Fat32Node : INode
    {
        public string FileName => _fileName;
        public long PhysicalSize => _size;
        public long FileSize => _size;

        public DateTime CreateTime => _cTime;
        public DateTime LastAccessTime => _aTime;
        public DateTime ModificationTime => _mTime;

        public async Task Flush(CancellationToken token) { }

        internal readonly Fat32 _fs;
        internal uint _cluster;
        internal uint _size;
        internal string _fileName;
        internal DateTime _cTime, _aTime, _mTime;


        internal Fat32Node(Fat32 fs, uint cluster, string filename, uint size, DateTime ctime, DateTime atime, DateTime mtime)
        {
            _fs = fs;
            _cluster = cluster;
            _fileName = filename;
            _size = size;
            _cTime = ctime;
            _aTime = atime;
            _mTime = mtime;
        }
    }

    class Fat32File : Fat32Node, IFile
    {
        long _currentClusterIdx = -1;
        Memory<byte> _currentCluster;

        internal Fat32File(Fat32 fs, uint cluster, string filename, uint size, DateTime c, DateTime a, DateTime m) : base(fs, cluster, filename, size, c, a, m) { }

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
                    buffer.Span[bytesRead + i] = _currentCluster.Span[index + i];
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
    }
    class Fat32Directory : Fat32Node, IDirectory
    {
        internal Fat32Directory(Fat32 fs, uint cluster, DateTime c, DateTime a, DateTime m) : base(fs, cluster, "", 0, c, a, m) { }

        internal Fat32Directory(Fat32 fs, uint cluster, string filename, DateTime c, DateTime a, DateTime m) : base(fs, cluster, filename, 0, c, a, m) { }

        public async Task<IFile> OpenFile(string filename, FileOpenMode mode, CancellationToken token = default)
        {
            Fat32File d = null;
            await _fs.Traverse(_cluster, TraversalFn);
            async Task<bool> TraversalFn(Fat32.DirentInfo f)
            {
                if (!f.IsDirectory && filename.Equals(f.Name))
                {
                    d = new(_fs, f.Cluster, f.Name, f.Size, f.CreatedAt, f.AccessedAt, f.ModifiedAt);
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
                    d = new(_fs, f.Cluster, f.Name, f.CreatedAt, f.AccessedAt, f.ModifiedAt);
                    return true;
                }
                return false;
            }
            return d;
        }

        public async Task<IFile> CreateFile(string name, DateTime creation, CancellationToken token = default)
        {
            var newCluster = await _fs.ClusterAllocate();
            await _fs.AddDirent(_cluster, name, creation, creation, creation, newCluster, (uint)_fs.BlockSize, false);
            return new Fat32File(_fs, newCluster, name, (uint)_fs.BlockSize, creation, creation, creation);
        }

        public async Task<IDirectory> CreateDirectory(string name, DateTime creation, CancellationToken token = default)
        {
            var newCluster = await _fs.ClusterAllocate();
            // TODO: zerofill cluster to be sure
            await _fs.AddDirent(_cluster, name, creation, creation, creation, newCluster, 0, true);
            await _fs.AddDirent(newCluster, ".", creation, creation, creation, newCluster, 0, true);
            await _fs.AddDirent(newCluster, "..", creation, creation, creation, _cluster, 0, true);
            return new Fat32Directory(_fs, newCluster, name, creation, creation, creation);
        }

        async Task DeleteDir(uint entCluster)
        {
            // delete dirent entries
            int start = 0, end = 0; uint direntCluster = 0;
            await _fs.Traverse(_cluster, TraversalFn);

            var entsPerCluster = _fs.BlockSize / 32;
            Memory<byte> mem = (await _fs.ReadCached(_fs.ClusterToLba(direntCluster)));
            for (int i = start; i <= end; i++)
            {
                var clusidx = (i / entsPerCluster);
                var clusoff = (int)(i % entsPerCluster);
                // read new cluster
                if (i != start && clusidx != ((i - 1) / entsPerCluster))
                {
                    await _fs._block.WriteBlocks(_fs.ClusterToLba(direntCluster), mem);
                    direntCluster = await _fs.NextInClusterChain(direntCluster);
                    mem = (await _fs.ReadCached(_fs.ClusterToLba(direntCluster)));
                }
                mem.Span[clusoff * 32] = 0xE5;
            }
            await _fs._block.WriteBlocks(_fs.ClusterToLba(direntCluster), mem);

            async Task<bool> TraversalFn(Fat32.DirentInfo f)
            {
                if (f.Cluster == entCluster)
                {
                    start = f.Start;
                    end = f.End;
                    direntCluster = f.DirentStartCluster;
                    return true;
                }
                return false;
            }
        }

        public async Task Delete(INode toDelete_, CancellationToken token = default)
        {
            Fat32Node toDelete = (Fat32Node)toDelete_;
            // free file clusterchain
            var cluster = toDelete._cluster;
            await _fs.FreeClusterChain(cluster);
            await DeleteDir(toDelete._cluster);
        }

        public async Task Rename(INode toRename_, string newName, CancellationToken token = default)
        {
            Fat32Node toRename = (Fat32Node)toRename_;
            await DeleteDir(toRename._cluster);
            bool isDir = toRename_ is Fat32Directory;
            await _fs.AddDirent(_cluster, newName, toRename._cTime, toRename._mTime, toRename._aTime, toRename._cluster, toRename._size, isDir);
        }

        public async Task Flush(CancellationToken token = default) { }
    }


    class Fat32 : IFileSystem
    {
        public static Fat32 stati;
        public long BlockSize => _bytesPerCluster;
        public bool ReadOnly => false;
        public long FreeSpace => 0;
        public string VolumeLabel => "";
        public long VolumeSize => 0;

        public async Task<IDirectory> OpenVolume()
        {
            var defTime = new DateTime(1980, 1, 1);
            return new Fat32Directory(this, _rootCluster, defTime, defTime, defTime);
        }

        internal IBlock _block;
        uint _rootCluster;
        internal uint _sectorsPerCluster;
        internal uint _bytesPerCluster;
        uint _firstFatSector;
        uint _firstDataSector;
        uint _sectorsPerFat;
        Dictionary<long, Memory<byte>> _cache;

        internal long ClusterToLba(uint cluster) => (cluster - 2) * _sectorsPerCluster + _firstDataSector;

        internal async Task<Memory<byte>> ReadCached(long lba)
        {
            if (_cache.ContainsKey(lba))
            {
                return _cache[lba];
            }
            else
            {
                var mem = MemoryServices.AllocatePages(1).Memory.Slice(0, (int)_bytesPerCluster);
                await _block.ReadBlocks(lba, mem);
                return mem;
            }
        }

        internal async Task<uint> NextInClusterChain(uint cluster)
        {
            var byteoff = (long)cluster * 4;
            var sector = _firstFatSector + byteoff / _block.BlockSize;
            var offset = (byteoff % _block.BlockSize) / 4;
            var fatSector = MemoryMarshal.Cast<byte, uint>((await ReadCached(sector)));
            return fatSector.Span[(int)offset] & 0x0FFFFFFF;
        }

        // TODO: cache writes
        internal async Task FreeClusterChain(uint cluster)
        {
            uint curr = cluster;
            var count = _block.BlockSize / 4;
            while (true)
            {
                var sector = _firstFatSector + curr / count;
                var offset = curr % count;
                var mem = (await ReadCached(sector));
                var fatSector = MemoryMarshal.Cast<byte, uint>(mem);
                curr = fatSector.Span[(int)offset] & 0x0FFFFFFF;
                if (curr >= 0x0FFFFFF8) break;
                
                var mask = 1 << ((int)(sector & 0b111));
                var index = unchecked((int)(sector / 8));
                _bitmap[index] &= (byte)(~mask);
                
                fatSector.Span[(int)offset] = 0;
                await _block.WriteBlocks(sector, mem);
            }
        }

        // TODO: this can be optimized dramatically, making use of FSInfo and also doing more than 1 op at the same time

        byte[] _bitmap;

        internal async Task<uint> ClusterAllocate()
        {
            uint newcluster = 0;
            for (int i = 0; i < _sectorsPerFat; i++)
            {
                if (_bitmap[i / 8] == 0xFF)
                {
                    // this can only happen for i being a multiple of 8
                    i += 7; // once this goes to i++, it will be +8
                    continue;
                }

                var mask = (byte)(1 << (i % 8));
                if ((_bitmap[i / 8] & mask) != 0)
                {
                    // this sector has been analyzed and has no free entries
                    continue;
                }

                var fatMem = ((await ReadCached(_firstFatSector + i)));
                var fatArr = MemoryMarshal.Cast<byte, uint>(fatMem);

                uint count = (uint)_block.BlockSize / 4;
                bool oneFree = false, moreFree = false;
                for (int j = 0; j < count; j++)
                {
                    if ((fatArr.Span[j] & 0x0FFFFFFF) == 0)
                    {
                        if (!oneFree)
                        {
                            // it's the first free entry
                            // update current FAT marking it as EOC
                            fatArr.Span[j] = (fatArr.Span[j] & 0xF0000000) | 0x0FFFFFFF; // end of clusterchain
                            newcluster = ((uint)i * count) + (uint)j;
                            await _block.WriteBlocks(_firstFatSector + i, fatMem); // update next end to be EOC
                            oneFree = true;
                        }
                        else
                        {
                            // it's the second free
                            moreFree = true;
                        }
                    }
                }
                if (!moreFree)
                {
                    // there was no free entry, or only one free entry (which we now filled)
                    // mark the sector as having no free entries anymore
                    _bitmap[i / 8] |= mask;
                }
                if (oneFree)
                {
                    return newcluster;
                }
            }
            return 0xFFFFFFFF;
        }
        internal async Task<uint> ClusterAllocate(uint cluster)
        {
            var newcluster = await ClusterAllocate();
            var count = _block.BlockSize / 4;

            var sector = _firstFatSector + cluster / count;
            var offset = cluster % count;
            var mem = (await ReadCached(sector));
            var fatSector = MemoryMarshal.Cast<byte, uint>(mem);
            // FIXME: check if it's EOC
            fatSector.Span[(int)offset] = (fatSector.Span[(int)offset] & 0xF0000000) | newcluster;

            await _block.WriteBlocks(sector, mem); // update current end

            return newcluster;
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
                _bytesPerCluster = (uint)block.BlockSize * sectorsPerCluster,
                _cache = new(),
                _bitmap = new byte[sectorsPerFat],
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
            var r = new Region((await ReadCached(ClusterToLba(cluster))));

            var lfnName = new char[64];
            var lfnLen = 0;
            int startIdx = 0; uint startCluster = 0;
            bool newDirentStart = true;
            var curr = cluster;
            int count = (int)(_sectorsPerCluster * _block.BlockSize) / 32;
            for (int clusidx = 0; ; clusidx++)
            {
                for (int i = 0; i < count; i++)
                {
                    if (newDirentStart)
                    {
                        startIdx = clusidx * count + i;
                        startCluster = curr;
                        newDirentStart = false;
                    }
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
                        newDirentStart = true;
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
                            ModifiedAt = FatToDateTime(sfn.mTime.Value),
                            AccessedAt = FatToDateTime((uint)sfn.aDate.Value << 16),
                            Size = sfn.fileSize.Value,
                            Start = startIdx,
                            DirentStartCluster = startCluster,
                            End = clusidx * count + i,
                        };

                        if (await callback(ent))
                        {
                            return;
                        }
                        lfnLen = 0;
                    }
                }
                curr = await NextInClusterChain(curr);
            }
        }

        internal async Task AddDirent(uint dirCluster, string name, DateTime creation, DateTime modify, DateTime access, uint startCluster, uint size, bool isDirectory)
        {
            bool isDot = name.Equals(".");
            bool isDotDot = name.Equals("..");
            bool onlySfn = isDot || isDotDot;

            var fatcTime = DateTimeToFat(creation);
            var fatmTime = DateTimeToFat(modify);
            var fataTime = DateTimeToFat(access);

            // how many contig entries do we need?
            var neededLfns = onlySfn ? 0 : ((name.Length + 13 - 1) / 13);
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

            var sfnName = new byte[11];
            byte sfnChecksum = 0;
            CalculateSfn();

            if (neededEnts > remaining)
            {
                // place all entries we can on the current block
                if (remaining > 0) await PlaceNEntries(remaining);

                // update entries to write and offset
                neededEnts -= remaining;
                offset = 0;

                // if the dirent straddles two clusters, place the remaining entries in the newly-allocated one
                // allocate new block
                dirCluster = await ClusterAllocate(dirCluster);
                r = new Region((await ReadCached(ClusterToLba(dirCluster))));

                // zerofill, TODO: improve
                for (int j = 0; j < _bytesPerCluster; j++) r.Span[j] = 0;
            }

            // place the last entries
            await PlaceNEntries(neededEnts);

            void CalculateSfn()
            {
                if (isDot)
                {
                    sfnName[0] = (byte)'.';
                    for (int j = 1; j < 11; j++) sfnName[j] = (byte)' ';
                }
                else if (isDotDot)
                {
                    sfnName[0] = sfnName[1] = (byte)'.';
                    for (int j = 2; j < 11; j++) sfnName[j] = (byte)' ';
                }
                else
                {
                    int curr = i;
                    sfnName[10] = sfnName[9] = sfnName[8] = 0x41;
                    for (int j = 0; j < 8; j++)
                    {
                        sfnName[7 - j] = (byte)(0x30 + curr % 10);
                        curr /= 10;
                    }
                }
                sfnChecksum = Checksum(sfnName);

            }

            async Task FindDirlistEnd()
            {
                while (true)
                {
                    i = 0;
                    var m = (await ReadCached(ClusterToLba(dirCluster)));
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
                    var next = await NextInClusterChain(dirCluster);
                    if (next >= 0x0FFFFFF8)
                    {
                        i = total;
                        return;
                    }
                    else
                    {
                        dirCluster = next;
                    }
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
                lfn.checksum.Value = sfnChecksum;
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
                for (int j = 0; j < 11; j++) sfn.name.Span[j] = sfnName[j];
                sfn.clusterHi.Value = (ushort)(startCluster >> 16);
                sfn.clusterLo.Value = (ushort)(startCluster & 0xFFFF);
                sfn.fileSize.Value = size;
                sfn.cTime.Value = fatcTime;
                sfn.aDate.Value = (ushort)(fataTime >> 16);
                sfn.mTime.Value = fatmTime;
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

        internal class DirentInfo
        {
            internal uint Cluster;
            internal string Name;
            internal bool IsDirectory;
            internal DateTime CreatedAt;
            internal DateTime ModifiedAt;
            internal DateTime AccessedAt;
            internal uint Size;

            internal uint DirentStartCluster;
            internal int Start, End;
        };

        private class Lfn
        {
            internal Field<byte> order;
            internal Field<byte> attr;
            internal Field<byte> type;
            internal Field<byte> checksum;
            internal Field<ushort> clusterLo;

            internal Memory<ushort> part1;
            internal Memory<ushort> part2;
            internal Memory<ushort> part3;

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

        private class Sfn
        {
            internal Memory<byte> name;
            internal Field<ushort> clusterHi;
            internal Field<ushort> clusterLo;
            internal Field<uint> fileSize;
            internal Field<uint> cTime;
            internal Field<ushort> aDate;
            internal Field<uint> mTime;
            internal Field<byte> attribs;

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
