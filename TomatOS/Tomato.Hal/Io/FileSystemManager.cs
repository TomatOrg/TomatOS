using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Threading;
using Tomato.Hal.Interfaces;

namespace Tomato.Hal.Io;

public class FileSystemManager
{

    private static readonly FileSystemManager Instance = new();
    private static bool _claimed = false;

    public static FileSystemManager Claim()
    {
        // fast path
        if (_claimed)
            throw new InvalidOperationException();

        // slow path
        lock (Instance)
        {
            if (_claimed)
                throw new InvalidOperationException();
            
            _claimed = true;
            return Instance;
        }
    }

    public static void Register(IFileSystem provider)
    {
        if (provider == null)
            throw new ArgumentNullException(nameof(provider));
        
        lock (Instance)
        {
            if (Instance._fileSystems != null)
            {
                Instance._fileSystems.Add(provider);
            }
            else
            {
                Instance._newFileSystemCallback(provider);
            }
        }
    }

    // Used to store the devices until someone claims them 
    private List<IFileSystem> _fileSystems = new();
    
    // the actions to call whenever there is a new device
    private Action<IFileSystem> _newFileSystemCallback = null;
    
    public Action<IFileSystem> NewFileSystemCallback
    {
        set
        {
            Debug.Assert(Monitor.IsEntered(this));
            
            if (_fileSystems != null)
            {
                foreach (var provider in _fileSystems)
                {
                    value(provider);
                }
                _fileSystems = null;
            }
            _newFileSystemCallback = value;
        }
    }

}