using System;
using System.Collections.Generic;
using Tomato.Hal.Interfaces;

namespace Tomato.Hal.Io;

public static class BlockManager
{

    private static bool _driversLocked = false;
    private static List<IFileSystemDriver> _drivers = new();
    private static List<IBlock> _blocks = new();

    /// <summary>
    /// Lock the BlockManager, prevents new drivers from being registered
    /// </summary>
    public static void Lock()
    {
        _driversLocked = true;
    }

    private static void DispatchBlock(IBlock block)
    {
        foreach (var driver in _drivers)
        {
            // try to parse the filesystem with this driver
            var fs = driver.TryCreate(block);
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
    /// Processes the block by checking if the block is partitioned
    /// </summary>
    /// <param name="block"></param>
    private static void ProcessBlock(IBlock block)
    {
        // TODO: check for GPT
        
        // TODO: check for MBR
        
        // process the block as un-partitioned
        DispatchBlock(block);
    }

    /// <summary>
    /// Register a new driver, trying to match it against the unmounted blocks
    /// </summary>
    public static void RegisterDriver(IFileSystemDriver driver)
    {
        if (_driversLocked)
            throw new InvalidOperationException();

        lock (_drivers)
        {
            _drivers.Add(driver);

            // go over the existing blocks trying to parse them
            for (var i = 0; i < _blocks.Count; i++)
            {
                var fs = driver.TryCreate(_blocks[i]);
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
            ProcessBlock(block);
        }
    }

}