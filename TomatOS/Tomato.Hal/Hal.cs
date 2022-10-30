using System;
using System.Diagnostics;
using System.Runtime.CompilerServices;
using Tomato.Hal.Acpi;
using Tomato.Hal.Drivers.PlainFramebuffer;
using Tomato.Hal.Managers;
using Tomato.Hal.Interfaces;

namespace Tomato.Hal;

public static class Hal
{

    #region Entry point
    
    private static bool _started = false;
    
    public static void Main()
    {
        // only allow entry once
        if (_started)
        {
            throw new InvalidOperationException();
        }
        _started = true;
    
        Debug.Print("Managed kernel is starting!");
        
        // all we need to do is call the acpi setup, everything will be
        // done on its own from that point forward
        AcpiManager.Init();
        
        // TODO: something better once we have real graphics acceleration support
        DisplayManager.RegisterGraphicsDevice(new PlainGraphicsDevice());
    }

    #endregion

    #region Native kernel resources
    
    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Native)]
    internal static extern ulong GetRsdp();

    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Native)]
    public static extern bool GetNextFramebuffer(ref int index, out ulong addr, out int width, out int height, out int pitch);
    
    #endregion

}