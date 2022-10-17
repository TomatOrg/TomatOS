using System;
using System.Diagnostics;
using System.Runtime.CompilerServices;
using System.Threading;
using Tomato.Hal.Acpi;
using Tomato.Hal.Drivers.PlainFramebuffer;
using Tomato.Hal.Managers;

namespace Tomato.Hal;

public static class Hal
{

    #region Entry point
    
    private static bool _started = false;
    
    public static void Main()
    {
        // make sure no one is trying to run us again
        if (_started)
            throw new InvalidOperationException();
        _started = true;
        
        Debug.Print("Managed kernel is starting!");
        
        // all we need to do is call the acpi setup, everything will be
        // done on its own from that point forward
        AcpiManager.Init();
        
        // add framebuffer 
        // TODO: figure how to handle when there is a normal graphics driver on
        //       the same pci device
        var plainGraphicsDevice = new PlainGraphicsDevice();
        if (plainGraphicsDevice.OutputsCount != 0)
        {
            DisplayManager.RegisterGraphicsDevice(plainGraphicsDevice);
        }

        ThreadPool.QueueUserWorkItem(state =>
        {
            Debug.Print("Hello from the thread pool!");
        });
        
        Debug.Print("Main thread does some work, then sleeps.");
        Thread.Sleep(1000);
        Debug.Print("We are done now");
    }

    #endregion

    #region Native kernel resources
    
    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Native)]
    internal static extern ulong GetRsdp();

    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Native)]
    public static extern bool GetNextFramebuffer(ref int index, out ulong addr, out int width, out int height, out int pitch);
    
    #endregion

}