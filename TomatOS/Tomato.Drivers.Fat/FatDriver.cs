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

namespace Tomato.Drivers.Fat;

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

    static internal Span<char> ConvertSfnToLfnString(Span<byte> sfn, char[] buf)
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

    static internal Span<char> TrimLfnString(char[] buf, int len)
    {
        for (; len > 0; len--) if (buf[len - 1] != 0xFFFF) break;
        if (buf[len - 1] == 0) len--;
        return new Span<char>(buf, 0, len);
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    internal struct DirentDateTime
    {
        internal uint Data;

        internal DirentDateTime(uint data) => Data = data;
        internal DirentDateTime(ushort data) => Data = ((uint)data) << 16;

        internal DirentDateTime(DateTime dt) :
              this(((uint)(dt.Year - 1980) << (16 + 9)) |
                   ((uint)dt.Month << (16 + 5)) |
                   ((uint)dt.Day << (16 + 0)) |
                   ((uint)dt.Hour << 11) |
                   ((uint)dt.Minute << 5) |
                   ((uint)(dt.Second / 2))) {}

        internal DateTime ToDateTime()
        {
            var hour = (int)(Data >> 11) & 0b11111;
            var min = (int)(Data >> 5) & 0b111111;
            var sec = (int)((Data >> 0) & 0b11111) * 2; // yes, the second field stores two-second intervals
            var year = (int)((Data >> (16 + 9)) & 0b1111111) + 1980;
            var month = (int)((Data >> (16 + 5)) & 0b1111); // january is stored as month 1, but so does C# DateTime
            var day = (int)((Data >> (16 + 0)) & 0b11111);
            return new DateTime(year, month, day, hour, min, sec);
        }
    }
}