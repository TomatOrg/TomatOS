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

internal static class Program
{
    public class FatFs : IFileSystem
    {
        internal FatFs()
        {
            Debug.Print("FatFs: Created volume");
        }
        public bool ReadOnly => true;
        public long VolumeSize => 0;
        public long FreeSpace => 0;
        public long BlockSize => 512;
        public string VolumeLabel => "Hello world";
        public Task<IDirectory> OpenVolume()
        {
            return null;
        }
    }
    public class FatDriver : IFileSystemDriver
    {
        // detecting a FAT filesystem is much harder than I imagined
        public async Task<IFileSystem> TryCreate(IBlock block)
        {
            var sector = MemoryServices.AllocatePhysicalMemory(512);
            await block.ReadBlocks(0, sector.Memory);
            var bootSectorJump = sector.Memory;
            var bpb = MemoryMarshal.Cast<byte, Bpb>(sector.Memory.Slice(3+8)).Span[0];
            if (bpb.Sectors != 0) bpb.LargeSectors = 0;
            if (bootSectorJump.Span[0] != 0xE9 && bootSectorJump.Span[0] != 0xEB && bootSectorJump.Span[0] != 0x49) return null;
            if (bpb.BytesPerSector !=  128 && bpb.BytesPerSector !=  256 && bpb.BytesPerSector !=  512 &&
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
            return new FatFs();
        }

        [StructLayout(LayoutKind.Sequential, Pack = 1)]
        struct Bpb
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
    }
    public static void Main()
    {
        var instance = new FatDriver();
        BlockManager.RegisterDriver(instance);
    }
}