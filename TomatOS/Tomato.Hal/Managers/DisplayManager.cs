using System;
using System.Collections.Generic;
using System.Threading;
using Tomato.Hal.Interfaces;

namespace Tomato.Hal.Managers;

public class DisplayManager
{

    private static DisplayManager _instance = new DisplayManager();
    private static bool _claimed = false;

    /// <summary>
    /// Claim the DisplayManager, only one person can do that and he should manage everything
    /// to do with new displays and input devices.
    /// </summary>
    public static DisplayManager Claim()
    {
        if (_claimed)
            throw new InvalidOperationException();
        _claimed = true;
        return _instance;
    }

    /// <summary>
    /// Register an new mouse to the display server
    /// </summary>
    public static void RegisterMouse(IRelMouse mouse)
    {
        lock (_instance)
        {
            _instance.Mice.Add(mouse);
            _instance.NewDevice.Set();
        }
        
    }

    /// <summary>
    /// Register a new keyboard to the display server
    /// </summary>
    public static void RegisterKeyboard(IKeyboard keyboard)
    {
        lock (_instance)
        {
            _instance.Keyboards.Add(keyboard);
            _instance.NewDevice.Set();
        }
    }

    /// <summary>
    /// Register a new graphics device to the display server
    /// </summary>
    public static void RegisterGraphicsDevice(IGraphicsDevice device)
    {
        lock (_instance)
        {
            _instance.GraphicsDevices.Add(device);
            _instance.NewDevice.Set();
        }
    }

    // NOTE: accessing these devices should be done while the manager is locked!
    
    public List<IRelMouse> Mice { get; } = new();
    public List<IKeyboard> Keyboards { get; } = new();
    public List<IGraphicsDevice> GraphicsDevices { get; } = new();

    /// <summary>
    /// Used to tell the owner that a new device was added, so it should check the devices again
    /// </summary>
    public ManualResetEvent NewDevice = new ManualResetEvent(false);
    
    private DisplayManager()
    {
    }

}