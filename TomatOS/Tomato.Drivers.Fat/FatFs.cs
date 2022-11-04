using System;
using System.Threading.Tasks;
using System.Threading;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Runtime.InteropServices;
using Tomato.Hal.Interfaces;
using Tomato.Hal;
using Tomato.Hal.Io;
using Tomato.Hal.Managers;
using Tomato.Hal.Pci;

namespace Tomato.Drivers.Virtio;

public class FatDirectory : FatNode, IDirectory
{
    public async Task<IFile> OpenFile(string filename, FileOpenMode mode, CancellationToken token = default)
    {
        FatFile file = null;
        await _fs.Traverse(_cluster, node =>
        {
            if (node._name.Equals(filename) && node is FatFile fileNode)
            {
                file = fileNode;
                return true;
            }
            return false;
        });
        return file;
    }

    public async Task<IDirectory> OpenDirectory(string filename, FileOpenMode mode, CancellationToken token = default)
    {
        FatDirectory dir = null;
        await _fs.Traverse(_cluster, node =>
        {
            if (node._name.Equals(filename) && node is FatDirectory dirNode)
            {
                dir = dirNode;
                return true;
            }
            return false;
        });
        return dir;
    }

    public IAsyncEnumerable<INode> GetAsyncEnumerator(CancellationToken token = default) => null;
}

public class FatFile : FatNode, IFile
{
    public Task<int> Read(long offset, Memory<byte> buffer, CancellationToken token) => Task.FromResult<int>(0);
    public Task<int> Write(long offset, ReadOnlyMemory<byte> buffer, CancellationToken token) => Task.FromResult<int>(0);
}

public class FatNode : INode
{
    public string FileName => _name;
    public long PhysicalSize => _size;
    public long FileSize => _size;
    public DateTime CreateTime => _cTime.ToDateTime();
    public DateTime LastAccessTime => _aTime.ToDateTime();
    public DateTime ModificationTime => _mTime.ToDateTime();

    internal FatFs _fs;
    internal uint _cluster;
    internal string _name;
    internal uint _size;
    internal FatDriver.DirentDateTime _aTime, _mTime, _cTime;
    internal uint _direntStartCluster, _direntEndCluster;
    internal int _direntStartIdx, _direntEndIdx;

    public Task Delete(CancellationToken token) => Task.CompletedTask;
    public Task Flush(CancellationToken token) => Task.CompletedTask;
}

public class FatFs : IFileSystem
{
    internal FatFs(IBlock block, FatDriver.Bpb bpb)
    {
        _block = block;
        _bpb = bpb;
        _clusterSize = _block.BlockSize * _bpb.SectorsPerCluster;
        _dataStart = _bpb.ReservedSectors + (_bpb.Fats * _bpb.LargeSectorsPerFat);
        Debug.Print("FatFs: Created volume");
        Test().Wait();

    }
    async Task Test()
    {
        var root = await OpenVolume();
        var dir = await root.OpenDirectory("boot", 0);
        var f = await dir.OpenFile("Tomato.Drivers.Fat.dll", 0);
        Debug.Print($"Tomato.Drivers.Fat.dll is {f.FileSize} bytes big");
    }

    public bool ReadOnly => true;
    public long VolumeSize => 0;
    public long FreeSpace => 0;
    public long BlockSize => 512;
    public string VolumeLabel => "Hello world";
    public Task<IDirectory> OpenVolume()
    {
        var ent = new FatDirectory();
        ent._fs = this;
        ent._cluster = _bpb.RootDirFirstCluster;
        return Task.FromResult<IDirectory>(ent);
    }

    internal IBlock _block;
    internal FatDriver.Bpb _bpb;
    internal int _clusterSize;
    internal long _dataStart;
    internal int _direntsPerCluster => (_bpb.SectorsPerCluster * _block.BlockSize) / 32;
    internal long ClusterToLba(uint cluster) => (cluster - 2) * _bpb.SectorsPerCluster + _dataStart;

    // TODO: use a disk pagecache
    async Task<Memory<byte>> Read(long sector, int bytes)
    {
        var mem = MemoryServices.AllocatePhysicalMemory(bytes).Memory;
        await _block.ReadBlocks(sector, mem);
        return mem;
    }

    internal async Task<uint> NextInClusterChain(uint cluster)
    {
        var byteoff = (long)cluster * 4;
        var sector = _bpb.ReservedSectors + byteoff / _block.BlockSize;
        var offset = (byteoff % _block.BlockSize) / 4;
        var fatSector = MemoryMarshal.Cast<byte, uint>((await Read(sector, _block.BlockSize)));
        return fatSector.Span[(int)offset] & 0x0FFFFFFF;
    }

    Span<char> ConvertSfnToLfnString(Span<byte> sfn, char[] buf)
    {
        // trim spaces from name
        int nameLen = 8;
        for (; nameLen > 0; nameLen--) if (sfn[nameLen - 1] != ' ') break;

        // trim spaces from extension
        int extLen = 3;
        for (; extLen > 0; extLen--) if (sfn[8 + (extLen - 1)] != ' ') break;

        if (extLen == 0)
        {
            for (int j = 0; j < nameLen; j++) buf[j] = (char)sfn[j];
            return new Span<char>(buf, 0, nameLen);
        }
        else
        {
            for (int j = 0; j < nameLen; j++) buf[j] = (char)sfn[j];
            for (int j = 0; j < extLen; j++) buf[nameLen + 1 + j] = (char)sfn[8 + j];
            buf[nameLen] = '.';
            return new Span<char>(buf, 0, nameLen + 1 + extLen);
        }
    }

    Span<char> TrimLfnString(char[] buf, int len)
    {
        for (; len > 0; len--) if (buf[len - 1] != 0xFFFF) break;
        if (buf[len - 1] == 0) len--;
        return new Span<char>(buf, 0, len);
    }

    internal async Task Traverse(uint cluster, Predicate<FatNode> cb)
    {
        // preallocate a buffer to use for all LFNs
        var lfnBuffer = new char[255]; // by spec, this is the 
        var lfnLen = 0;

        // cluster:index position of the starting SFN in the dirent.
        // this is necessary because a dirent can span two clusters
        uint startCluster = cluster, currCluster;
        int startIdx = 0, currIdx;
        bool newDirentStart = true;

        for (currCluster = startCluster; currCluster < 0x0FFFFFF8;)
        {
            var clusterData = await Read(ClusterToLba(currCluster), _clusterSize);
            for (currIdx = 0; currIdx < _direntsPerCluster; currIdx++)
            {
                if (newDirentStart)
                {
                    lfnLen = 0;
                    startIdx = currIdx;
                    startCluster = currCluster;
                    newDirentStart = false;
                }

                var direntBytes = new Region(clusterData.Slice(currIdx * 32, 32));
                // There's an LFN. TODO: actually use the checksum to make sure that it's the right LFN.
                // DOS will ignore LFNs (by design) and may rearrange the SFN entry so it doesn't come immediately
                if (direntBytes.Span[11] == 0xF)
                {
                    var lfn = new FatDriver.Lfn(direntBytes);
                    int start = ((lfn.Order.Value & (~0x40)) - 1) * 13;
                    lfnLen = Math.Max(lfnLen, start + 13);
                    for (int j = 0; j < lfn.Part1.Length; j++) lfnBuffer[start + j] = (char)lfn.Part1.Span[j];
                    for (int j = 0; j < lfn.Part2.Length; j++) lfnBuffer[start + 5 + j] = (char)lfn.Part2.Span[j];
                    for (int j = 0; j < lfn.Part3.Length; j++) lfnBuffer[start + 11 + j] = (char)lfn.Part3.Span[j];
                }
                else
                {
                    newDirentStart = true;
                    var sfn = new FatDriver.Sfn(direntBytes);

                    if (sfn.Name.Span[0] == 0) break;
                    if (sfn.Name.Span[0] == '.' || sfn.Name.Span[0] == 0xE5) continue; // exclude dot, dotdot, UNIX-like hidden entries (starting with .) and deleted files (0xE5)

                    FatNode ent;
                    if ((sfn.Attribs.Value & 0x10) != 0) ent = new FatDirectory();
                    else ent = new FatFile();

                    ent._fs = this;
                    ent._name = ((lfnLen > 0) ? TrimLfnString(lfnBuffer, lfnLen) : ConvertSfnToLfnString(sfn.Name.Span, lfnBuffer)).ToString();
                    ent._cTime = new(sfn.CTime.Value);
                    ent._mTime = new(sfn.MTime.Value);
                    ent._aTime = new(sfn.ADate.Value);
                    ent._cluster = sfn.Cluster;
                    ent._size = sfn.FileSize.Value;
                    ent._direntStartCluster = startCluster;
                    ent._direntStartIdx = startIdx;
                    ent._direntEndCluster = currCluster;
                    ent._direntEndIdx = currIdx;
                    if (cb(ent)) return;
                }
            }
            currCluster = await NextInClusterChain(currCluster);
        }
    }
}

