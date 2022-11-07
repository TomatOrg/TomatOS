using System;
using System.Collections.Generic;
using System.Threading;
using Tomato.Hal.Interfaces;

namespace Tomato.Hal.Io;

public class FileSystemManager
{
    
    private static FileSystemManager _instance = new FileSystemManager();
    private static bool _claimed = false;

    public static FileSystemManager Claim()
    {
        if (_claimed)
            throw new InvalidOperationException();
        _claimed = true;
        return _instance;
    }

    /// <summary>
    /// Register an new filesystem to the file system manager
    /// </summary>
    public static void Register(IFileSystem fs)
    {
        lock (_instance)
        {
            _instance.FileSystems.Add(fs);
            _instance.NewFileSystem.Set();
        }
        
    }

    // NOTE: accessing these devices should be done while the manager is locked!
    
    public List<IFileSystem> FileSystems { get; } = new();

    /// <summary>
    /// Used to tell the owner that a new device was added, so it should check the devices again
    /// </summary>
    public AutoResetEvent NewFileSystem = new AutoResetEvent(false);
    
    private FileSystemManager()
    {
    }
    
}