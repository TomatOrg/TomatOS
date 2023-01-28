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

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct TableHeader
    {
        public ulong Signature;
        public uint Revision;
        public uint HeaderSize;
        public uint Crc32;
        public uint Reserved;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct PartitionTableHeader
    {
        public TableHeader Header;
        public long MyLba;
        public long AlternateLba;
        public long FirstUsableLba;
        public long LastUsableLba;
        public Guid DiskGuid;
        public long PartitionEntryLba;
        public uint NumberOfPartitionEntries;
        public uint SizeOfPartitionEntry;
        public uint PartitionEntryArrayCrc32;
    }

    // TODO: figure how to use this nicely
    [StructLayout(LayoutKind.Sequential, Size = 36 * sizeof(char))]
    public struct PartitionEntryName
    {
    }
    
    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct PartitionEntry
    {
        public Guid PartitionTypeGuid;
        public Guid UniquePartitionGuid;
        public long StartingLba;
        public long EndingLba;
        public ulong Attributes;
        public PartitionEntryName PartitionName;
    }

    private static bool CheckGptTableHeader(long lba, Memory<byte> header)
    {
        ref var partHeader = ref MemoryMarshal.Cast<byte, PartitionTableHeader>(header.Span)[0];
        // TODO: CRC check
        return partHeader.Header.Signature != 0x5452415020494645 ||
               partHeader.MyLba != lba ||
               partHeader.SizeOfPartitionEntry < Unsafe.SizeOf<PartitionEntry>();
    }
    
    private static async Task<bool> ValidateGptTable(Memory<byte> data, long lba, IBlock block)
    {
        // read the data, 
        await block.ReadBlocks(lba, data);
        
        // validate the header in another place so we can use ref stuff
        if (CheckGptTableHeader(lba, data))
            return false;
        
        // TODO: validate partition entry array crc

        return true;
    }

    private static long GetAlternateLba(Memory<byte> data)
    {
        return MemoryMarshal.Cast<byte, PartitionTableHeader>(data.Span)[0].AlternateLba;
    }
    
    /// <summary>
    /// Takes an IBlock and checks if it is GPT formatted 
    /// </summary>
    public static async Task<bool> IsGpt(IBlock block)
    {
        // TODO: verify protective MBR is valid

        var primaryHeaderData = MemoryServices.AllocatePhysicalMemory(block.BlockSize).Memory;
        var backupHeaderData = MemoryServices.AllocatePhysicalMemory(block.BlockSize).Memory;
        
        // check primary gpt table
        if (!await ValidateGptTable(primaryHeaderData, 1, block))
        {
            // No primary gpt table, check the backup just in case
            if (!await ValidateGptTable(backupHeaderData, block.LastBlock, block))
            {
                // no valid backup, probably just not GPT formatted 
                return false;
            }
            else
            {
                // TODO: if we got a valid backup we can try and restore the 
                //       primary gpt table
                return false;
            }
        }
        else if (!await ValidateGptTable(backupHeaderData, GetAlternateLba(primaryHeaderData), block)) 
        { 
            // we got a valid primary lba but not a valid backup lba
            
            // TODO: try to restore the backup
        }
        
        return true;
    }

    /// <summary>
    /// Tries to parse the GPT table 
    /// </summary>
    public static async IAsyncEnumerable<BlockManager.GenericPartition> IteratePartitions(IBlock block)
    {
        // read the primary partition table, verifying it again just in case
        var primaryHeaderData = MemoryServices.AllocatePhysicalMemory(block.BlockSize).Memory;
        if (!await ValidateGptTable(primaryHeaderData, 1, block))
            yield break;
        
        var partHeader = MemoryMarshal.Read<PartitionTableHeader>(primaryHeaderData.Span);
        
        // allocate the array for reading the partitions 
        var sizeOfEntry = partHeader.SizeOfPartitionEntry;
        var partEntryData = MemoryServices.AllocatePhysicalMemory((int)(partHeader.NumberOfPartitionEntries *
                                                                        sizeOfEntry)).Memory;
        await block.ReadBlocks(partHeader.PartitionEntryLba, partEntryData);
        
        // iterate all the partitions 
        for (ulong i = 0; i < partHeader.NumberOfPartitionEntries; i++)
        {
            // read the entry 
            var entry = MemoryMarshal.Read<PartitionEntry>(partEntryData.Span.Slice((int)(sizeOfEntry * i), (int)sizeOfEntry));
            
            // ignore empty partitions
            if (entry.PartitionTypeGuid == Guid.Empty)
            {
                continue;
            }
            
            // create the generic partition
            yield return new BlockManager.GenericPartition(block, entry.StartingLba, entry.EndingLba);
        }
    }
    
}