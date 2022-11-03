using System;
using System.Runtime.InteropServices;
using System.Buffers;
using System.Threading;
using System.Diagnostics;
using System.Collections.Generic;
using System.Threading.Tasks;
using System.Runtime.CompilerServices;
using Tomato.Hal.Interfaces;
using Tomato.Hal.Managers;
using Tomato.Hal.Pci;
using Tomato.Hal.Io;
using Tomato.Hal;
namespace Tomato.Hal.Io;

public static class Gpt 
{
    static public async Task<bool> IteratePartitions(IBlock disk, Action<BlockManager.GenericPartition> addCb)
    {
        bool foundNoPartitions = true;
        var mem = MemoryServices.AllocatePhysicalMemory(disk.BlockSize).Memory;
        await disk.ReadBlocks(1, mem);
        var region = new Region(mem);
        var hdr = new Header(region);
        ulong currentlyReadLba = 1;
        for (ulong i = 0; i < hdr.Entries; i++)
        {
            ulong idx = i * hdr.EntrySize;
            ulong lba = hdr.PartitionsLba + idx / (ulong)disk.BlockSize;
            ulong offset = idx % (ulong)disk.BlockSize;
            if (lba != currentlyReadLba)
            {
                await disk.ReadBlocks((long)lba, mem);
                currentlyReadLba = lba;
            }
            var part = new PartitionEntry(region.CreateRegion((int)offset));
            if (!part.IsEmpty)
            {
                foundNoPartitions = false;
                Debug.Print($"BlockManager.Gpt: part{i} at LBAs 0x{part.StartLba:x0}-0x{part.EndLba:x}");
                addCb(new BlockManager.GenericPartition(disk, (long)part.StartLba, (long)part.EndLba));
            }
        }
        return foundNoPartitions;
    }
    class PartitionEntry
    {
        public byte[] TypeGuid;
        public byte[] UniqueGuid;
        public ulong StartLba;
        public ulong EndLba;
        public ulong Attribs;
        public byte[] PartName;
        public bool IsEmpty;
        public PartitionEntry(Region r)
        {
            TypeGuid = r.CreateMemory<byte>(0, 16).ToArray();
            UniqueGuid = r.CreateMemory<byte>(0x10, 16).ToArray();
            StartLba = r.CreateField<ulong>(0x20).Value;
            EndLba = r.CreateField<ulong>(0x28).Value;
            Attribs = r.CreateField<ulong>(0x30).Value;
            PartName = r.CreateMemory<byte>(0x38, 72).ToArray();
            IsEmpty = true;
            for (int j = 0; j < 16; j++) if (TypeGuid[j] != 0)
            {
                IsEmpty = false;
                break;
            }
        }
    };

    class Header
    {
        public uint Revision;
        public uint HeaderSize;
        public uint HeaderCrc;
        public ulong ThisLba;
        public ulong AlternateLba;
        public ulong FirstUsableBlock;
        public ulong LastUsableBlock;
        public byte[] DiskGuid;
        public ulong PartitionsLba;
        public uint Entries;
        public uint EntrySize;
        public uint PartitionsCrc;
        public Header(Region r)
        {
            Revision = r.CreateField<uint>(0x8).Value;
            HeaderSize = r.CreateField<uint>(0xC).Value;
            HeaderCrc = r.CreateField<uint>(0x10).Value;
            ThisLba = r.CreateField<ulong>(0x18).Value;
            AlternateLba = r.CreateField<ulong>(0x20).Value;
            FirstUsableBlock = r.CreateField<ulong>(0x28).Value;
            LastUsableBlock = r.CreateField<ulong>(0x30).Value;
            DiskGuid = r.CreateMemory<byte>(0x38).ToArray();
            PartitionsLba = r.CreateField<ulong>(0x48).Value;
            Entries = r.CreateField<uint>(0x50).Value;
            EntrySize = r.CreateField<uint>(0x54).Value;
            PartitionsCrc = r.CreateField<uint>(0x58).Value;
        }
    }
}