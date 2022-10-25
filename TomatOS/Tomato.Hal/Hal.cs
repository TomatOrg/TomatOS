using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;
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

        Task.Run(MyMethod).Wait();
    }

    static async Task MyMethod()
    {
        Coffee cup = PourCoffee();
        Debug.WriteLine("coffee is ready");

        var eggsTask = FryEggsAsync(2);
        var baconTask = FryBaconAsync(3);
        var toastTask = MakeToastWithButterAndJamAsync(2);

        var breakfastTasks = new List<Task> { eggsTask, baconTask, toastTask };
        while (breakfastTasks.Count > 0)
        {
            Task finishedTask = await Task.WhenAny(breakfastTasks);
            if (finishedTask == eggsTask)
            {
                Debug.WriteLine("eggs are ready");
            }
            else if (finishedTask == baconTask)
            {
                Debug.WriteLine("bacon is ready");
            }
            else if (finishedTask == toastTask)
            {
                Debug.WriteLine("toast is ready");
            }
            breakfastTasks.Remove(finishedTask);
        }

        Juice oj = PourOJ();
        Debug.WriteLine("oj is ready");
        Debug.WriteLine("Breakfast is ready!");
    }
    
    internal class Bacon { }
    internal class Coffee { }
    internal class Egg { }
    internal class Juice { }
    internal class Toast { }
    
    static async Task<Toast> MakeToastWithButterAndJamAsync(int number)
        {
            var toast = await ToastBreadAsync(number);
            ApplyButter(toast);
            ApplyJam(toast);

            return toast;
        }

        private static Juice PourOJ()
        {
            Debug.WriteLine("Pouring orange juice");
            return new Juice();
        }

        private static void ApplyJam(Toast toast) =>
            Debug.WriteLine("Putting jam on the toast");

        private static void ApplyButter(Toast toast) =>
            Debug.WriteLine("Putting butter on the toast");

        private static async Task<Toast> ToastBreadAsync(int slices)
        {
            for (int slice = 0; slice < slices; slice++)
            {
                Debug.WriteLine("Putting a slice of bread in the toaster");
            }
            Debug.WriteLine("Start toasting...");
            await Task.Delay(3000);
            Debug.WriteLine("Remove toast from toaster");

            return new Toast();
        }

        private static async Task<Bacon> FryBaconAsync(int slices)
        {
            Debug.WriteLine($"putting {slices} slices of bacon in the pan");
            Debug.WriteLine("cooking first side of bacon...");
            await Task.Delay(3000);
            for (int slice = 0; slice < slices; slice++)
            {
                Debug.WriteLine("flipping a slice of bacon");
            }
            Debug.WriteLine("cooking the second side of bacon...");
            await Task.Delay(3000);
            Debug.WriteLine("Put bacon on plate");

            return new Bacon();
        }

        private static async Task<Egg> FryEggsAsync(int howMany)
        {
            Debug.WriteLine("Warming the egg pan...");
            await Task.Delay(3000);
            Debug.WriteLine($"cracking {howMany} eggs");
            Debug.WriteLine("cooking the eggs ...");
            await Task.Delay(3000);
            Debug.WriteLine("Put eggs on plate");

            return new Egg();
        }

        private static Coffee PourCoffee()
        {
            Debug.WriteLine("Pouring coffee");
            return new Coffee();
        }

    #endregion

    #region Native kernel resources
    
    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Native)]
    internal static extern ulong GetRsdp();

    [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Native)]
    public static extern bool GetNextFramebuffer(ref int index, out ulong addr, out int width, out int height, out int pitch);
    
    #endregion

}