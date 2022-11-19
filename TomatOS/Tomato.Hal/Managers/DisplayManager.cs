using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Threading;
using Tomato.Hal.Interfaces;

namespace Tomato.Hal.Managers;

public class DisplayManager
{

    private static readonly DisplayManager Instance = new();
    private static bool _claimed = false;

    /// <summary>
    /// Claim the DisplayManager, only one person can do that and he should manage everything
    /// to do with new displays and input devices.
    /// </summary>
    public static DisplayManager Claim()
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

    /// <summary>
    /// Register an new mouse to the display server
    /// </summary>
    public static void RegisterMouse(IRelMouse mouse)
    {
        if (mouse == null)
            throw new ArgumentNullException(nameof(mouse));
        
        lock (Instance)
        {
            if (Instance._relMice != null)
            {
                Instance._relMice.Add(mouse);
            }
            else
            {
                Instance._newRelMouseCallback(mouse);
            }
        }
        
    }

    /// <summary>
    /// Register a new keyboard to the display server
    /// </summary>
    public static void RegisterKeyboard(IKeyboard keyboard)
    {
        if (keyboard == null)
            throw new ArgumentNullException(nameof(keyboard));
        
        lock (Instance)
        {            
            if (Instance._keyboards != null)
            {
                Instance._keyboards.Add(keyboard);
            }
            else
            {
                Instance._keyboardCallback(keyboard);
            }
        }
    }

    /// <summary>
    /// Register a new graphics device to the display server
    /// </summary>
    public static void RegisterGraphicsDevice(IGraphicsDevice device)
    {
        if (device == null)
            throw new ArgumentNullException(nameof(device));

        lock (Instance)
        {
            if (Instance._relMice != null)
            {
                Instance._graphicsDevices.Add(device);
            }
            else
            {
                Instance._newGraphicsDeviceCallback(device);
            }
        }
    }

    // Used to store the devices until someone claims them 
    private List<IRelMouse> _relMice = new();
    private List<IKeyboard> _keyboards = new();
    private List<IGraphicsDevice> _graphicsDevices = new();

    // the actions to call whenever there is a new device
    private Action<IRelMouse> _newRelMouseCallback = null;
    private Action<IKeyboard> _keyboardCallback = null;
    private Action<IGraphicsDevice> _newGraphicsDeviceCallback = null;
    
    public Action<IRelMouse> NewRelMouseCallback
    {
        set
        {
            Debug.Assert(Monitor.IsEntered(this));
            
            if (_relMice != null)
            {
                foreach (var mice in _relMice)
                {
                    value(mice);
                }
                _relMice = null;
            }
            _newRelMouseCallback = value;
        }
    }
    
    public Action<IKeyboard> NewKeyboardCallback
    {
        set
        {
            Debug.Assert(Monitor.IsEntered(this));

            if (_keyboards != null)
            {
                foreach (var keyboard in _keyboards)
                {
                    value(keyboard);
                }
                _keyboards = null;
            }
            _keyboardCallback = value;
        }
    }
    
    public Action<IGraphicsDevice> NewGraphicsDeviceCallback
    {
        set
        {
            Debug.Assert(Monitor.IsEntered(this));

            if (_graphicsDevices != null)
            {
                foreach (var device in _graphicsDevices)
                {
                    value(device);
                }
                _graphicsDevices = null;
            }
            _newGraphicsDeviceCallback = value;
        }
    }

    private DisplayManager()
    {
    }

}