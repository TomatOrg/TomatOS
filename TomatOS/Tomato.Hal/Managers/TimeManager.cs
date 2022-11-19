using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Threading;
using TinyDotNet;
using Tomato.Hal.Interfaces;

namespace Tomato.Hal.Managers;

public class TimeManager
{

    private static TimeManager _instance = new TimeManager();
    private static bool _claimed = false;

    public static TimeManager Claim()
    {
        // fast path
        if (_claimed)
            throw new InvalidOperationException();

        // slow path
        lock (_instance)
        {
            if (_claimed)
                throw new InvalidOperationException();
            
            _claimed = true;
            return _instance;
        }
    }

    public static void RegisterTimeProvider(ITimeProvider provider)
    {
        if (provider == null)
            throw new ArgumentNullException(nameof(provider));
        
        lock (_instance)
        {
            _instance.DefaultProvider ??= provider;

            if (_instance._timeProviders != null)
            {
                _instance._timeProviders.Add(provider);
            }
            else
            {
                _instance._newTimeProviderCallback(provider);
            }
        }
    }

    public static ITimeProvider GetDefaultTimeProvider()
    {
        lock (_instance)
        {
            return _instance.DefaultProvider;
        }
    }

    private ITimeProvider _defaultProvider = null;
    public ITimeProvider DefaultProvider
    {
        get => _defaultProvider;
        set => _defaultProvider = value ?? throw new ArgumentNullException(nameof(value));
    }

    // Used to store the devices until someone claims them 
    private List<ITimeProvider> _timeProviders = new();
    
    // the actions to call whenever there is a new device
    private Action<ITimeProvider> _newTimeProviderCallback = null;
    
    public Action<ITimeProvider> NewTimeProviderCallback
    {
        set
        {
            Debug.Assert(Monitor.IsEntered(this));
            
            if (_timeProviders != null)
            {
                foreach (var provider in _timeProviders)
                {
                    value(provider);
                }
                _timeProviders = null;
            }
            _newTimeProviderCallback = value;
        }
    }

}