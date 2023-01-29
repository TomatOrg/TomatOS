using System;
using System.Threading;

namespace Tomato.App;

public class App
{

    /// <summary>
    /// Stores the current app
    /// </summary>
    private static AsyncLocal<App> _current = new();

    /// <summary>
    /// Get the current app 
    /// </summary>
    public static App Current => _current.Value;

    /// <summary>
    /// The name of the app, this is not human readable
    /// </summary>
    public string Name { get; private set; }

    /// <summary>
    /// The default capabilities this app has 
    /// </summary>
    internal CapabilityDomain _defaultCapabilities;

    /// <summary>
    /// Create the app that represents the kernel, it will
    /// have a root capability
    /// </summary>
    internal static void CreateKernelApp()
    {
        var app = new App();
        app.Name = "TomatOS";
        app._defaultCapabilities = CapabilityDomain.CreateRoot();
        _current.Value = app;
    }
    
}