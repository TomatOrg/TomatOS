using System;
using System.Runtime.InteropServices;
using System.Buffers;
using System.Threading;
using System.Diagnostics;
using System.Collections.Generic;
using System.Threading.Tasks;
using System.Runtime.CompilerServices;
using Tomato.App;
using Tomato.Hal.Interfaces;
using Tomato.Hal.Managers;
using Tomato.Hal.Pci;
using Tomato.Hal.Io;
using Tomato.Hal;
namespace Tomato.Hal.Io;

public static class BlockManager
{

    private static List<IFileSystemDriver> _drivers = new();
    private static List<IBlock> _blocks = new();

    public static Capability FileSystemDriver = new Capability("FileSystem driver", "Allows to act as a filesystem driver, with raw access to the partitions on the disk");

    /// <summary>
    /// Dispatch a single block, assuming that this block 
    /// </summary>
    /// <param name="block"></param>
    private static async Task DispatchBlock(IBlock block)
    {
        foreach (var driver in _drivers)
        {
            // try to parse the filesystem with this driver
            var fs = await driver.TryCreate(block);
            if (fs == null) 
                continue;
                
            // success! no need to continue
            FileSystemManager.Register(fs);
            return;
        }
        
        // no driver yet, store for future reference
        _blocks.Add(block);
    }

    /// <summary>
    /// Processes the block by checking if the block is partitioned, this
    /// assumes that we are already with the lock
    /// </summary>
    private static async Task ProcessBlock(IBlock block)
    {
        // check for GPT
        if (await Gpt.IsGpt(block))
        {
            await foreach (var part in Gpt.IteratePartitions(block))
            {
                await DispatchBlock(part);
            }
        }
        // TODO: check for MBR
        else
        {
            // process the block as un-partitioned
            await DispatchBlock(block);
        }
    }

    /// <summary>
    /// Register a new driver, trying to match it against the unmounted blocks
    /// </summary>
    public static void RegisterDriver(IFileSystemDriver driver)
    {
        CapabilityDomain.Ensure(FileSystemDriver);

        lock (_drivers)
        {
            _drivers.Add(driver);

            // go over the existing blocks trying to parse them
            for (var i = 0; i < _blocks.Count; i++)
            {
                var fs = driver.TryCreate(_blocks[i]).Result;
                if (fs == null) 
                    continue;
                
                FileSystemManager.Register(fs);
                _blocks.RemoveAt(i);
                i--;
            }
        }
    }
    
    /// <summary>
    /// Register a new block device, matching it against all the drivers
    /// </summary>
    /// <param name="block"></param>
    public static void RegisterBlock(IBlock block)
    {
        lock (_drivers)
        {
            ProcessBlock(block).Wait();
        }
    }

}