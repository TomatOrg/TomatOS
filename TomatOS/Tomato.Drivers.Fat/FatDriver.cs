using System;
using System.Threading.Tasks;
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

internal class FatDriver : IFileSystemDriver
{
    // detecting a FAT filesystem is much harder than I imagined
    public async Task<IFileSystem> TryCreate(IBlock block)
    {
        var sector = MemoryServices.AllocatePhysicalMemory(512);
        await block.ReadBlocks(0, sector.Memory);
        var bootSectorJump = sector.Memory;
        var bpb = MemoryMarshal.Cast<byte, Bpb>(sector.Memory.Slice(3 + 8)).Span[0];
        if (bpb.Sectors != 0) bpb.LargeSectors = 0;
        if (bootSectorJump.Span[0] != 0xE9 && bootSectorJump.Span[0] != 0xEB && bootSectorJump.Span[0] != 0x49) return null;
        if (bpb.BytesPerSector != 128 && bpb.BytesPerSector != 256 && bpb.BytesPerSector != 512 &&
            bpb.BytesPerSector != 1024 && bpb.BytesPerSector != 2048 && bpb.BytesPerSector != 4096) return null;
        if (bpb.SectorsPerCluster != 1 && bpb.SectorsPerCluster != 2 && bpb.SectorsPerCluster != 4 && bpb.SectorsPerCluster != 8 &&
            bpb.SectorsPerCluster != 16 && bpb.SectorsPerCluster != 32 && bpb.SectorsPerCluster != 64 && bpb.SectorsPerCluster != 128) return null;
        if (bpb.ReservedSectors == 0) return null;
        if (bpb.Fats == 0) return null;
        if (bpb.Sectors == 0 && bpb.LargeSectors == 0) return null;
        if (bpb.SectorsPerFat == 0 && (bpb.LargeSectorsPerFat == 0 || bpb.FsVersion != 0)) return null;
        if ((bpb.Media != 0xf0) && (bpb.Media != 0xf8) && (bpb.Media != 0xf9) && (bpb.Media != 0xfb) &&
           (bpb.Media != 0xfc) && (bpb.Media != 0xfd) && (bpb.Media != 0xfe) && (bpb.Media != 0xff)) return null;
        if (bpb.SectorsPerFat != 0 && bpb.RootEntries == 0) return null;
        if (bpb.SectorsPerFat == 0 && bpb.MirrorDisabled) return null;
        return new FatFs(block, bpb);
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    internal struct Bpb
    {
        internal ushort BytesPerSector;
        internal byte SectorsPerCluster;
        internal ushort ReservedSectors;
        internal byte Fats;
        internal ushort RootEntries;
        internal ushort Sectors;
        internal byte Media;
        internal ushort SectorsPerFat;
        internal ushort SectorsPerTrack;
        internal ushort Heads;
        internal uint HiddenSectors;
        internal uint LargeSectors;
        internal uint LargeSectorsPerFat;
        internal ushort ExtendedFlags;
        internal ushort FsVersion;
        internal uint RootDirFirstCluster;
        internal ushort FsInfoSector;
        internal ushort BackupBootSector;

        internal bool MirrorDisabled => (ExtendedFlags >> 7) != 0;
    }

    internal class Lfn
    {
        internal Field<byte> Order;
        internal Field<byte> Attr;
        internal Field<byte> Type;
        internal Field<byte> Checksum;
        internal Field<ushort> ClusterLo;

        internal Memory<ushort> Part1;
        internal Memory<ushort> Part2;
        internal Memory<ushort> Part3;

        internal Lfn(Region r)
        {
            Order = r.CreateField<byte>(0);
            Attr = r.CreateField<byte>(11);
            Type = r.CreateField<byte>(12);
            Checksum = r.CreateField<byte>(13);
            ClusterLo = r.CreateField<ushort>(26);
            Part1 = r.CreateMemory<ushort>(1, 5);
            Part2 = r.CreateMemory<ushort>(14, 6);
            Part3 = r.CreateMemory<ushort>(28, 2);
        }
    }

    internal class Sfn
    {
        internal Memory<byte> Name;
        internal Field<ushort> ClusterHi;
        internal Field<ushort> ClusterLo;
        internal Field<uint> FileSize;
        internal Field<uint> CTime;
        internal Field<ushort> ADate;
        internal Field<uint> MTime;
        internal Field<byte> Attribs;

        internal uint Cluster => ((uint)ClusterHi.Value << 16) | ClusterLo.Value;

        internal Sfn(Region r)
        {
            Name = r.CreateMemory<byte>(0, 11);
            ClusterHi = r.CreateField<ushort>(20);
            ClusterLo = r.CreateField<ushort>(26);
            FileSize = r.CreateField<uint>(28);
            CTime = r.CreateField<uint>(14);
            ADate = r.CreateField<ushort>(18);
            MTime = r.CreateField<uint>(22);
            Attribs = r.CreateField<byte>(11);
        }
    }
}