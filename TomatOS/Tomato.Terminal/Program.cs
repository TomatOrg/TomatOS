using System;
using System.Threading;
using System.Threading.Tasks;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Runtime.InteropServices;
using Tomato.Graphics;
using Tomato.Hal.Interfaces;
using Tomato.Hal.Managers;
using Tomato.Hal.Io;

namespace Tomato.Terminal;

internal static class Program
{

    private static DisplayManager _displayManager;
    private static Terminal _terminal;

    public static void Main()
    {
        try
        {
            _displayManager = DisplayManager.Claim();
        }
        catch (InvalidOperationException)
        {
            Debug.Print("Failed to claim the display manager!");
            return;
        }

        // get the keyboard
        var keyboard = _displayManager.Keyboards[0];

        // get the graphics device
        var dev = _displayManager.GraphicsDevices[0];
        var output = dev.Outputs[0];

        // create a gpu framebuffer and attach it to the first output
        var framebuffer = dev.CreateFramebuffer(output.Width, output.Height);
        output.SetFramebuffer(framebuffer, new Rectangle(0, 0, output.Width, output.Height));

        // allocate a cpu backing 
        var m = new byte[framebuffer.Width * framebuffer.Height * 4].AsMemory();
        var memory = MemoryMarshal.Cast<byte, uint>(m);
        framebuffer.Backing = m;

        _terminal = new Terminal(framebuffer, memory, keyboard, new Font(Typeface.Default, 16));
        _terminal.Insert("Welcome to TomatOS!");
        _terminal.InsertNewLine();

        (new Thread(() => Shell().Wait())).Start();
    }

    static async Task Shell()
    {
        var fsm = FileSystemManager.Claim();
        fsm.NewFileSystem.WaitOne();
        var fs = fsm.FileSystems[0];

        var root = fs.OpenVolume().Result;
        var cwd = root;
        _terminal.Insert($"Loaded rootfs with label \"{fs.VolumeLabel}\".");
        _terminal.InsertNewLine();
        while (true)
        {
            _terminal.Insert("> ");
            var command = _terminal.ReadLine();
            var commandArr = command.Split(' ');
            switch (commandArr[0])
            {
                case "ls": await CmdList(commandArr); break;
                default: await CmdNotFound(commandArr); break;
            }
        }

        async Task CmdList(string[] arr)
        {
            if (arr.Length > 2)
            {
                _terminal.Insert("ERROR: wrong syntax for \"ls\".");
                _terminal.InsertNewLine();
                return;
            }
            var dir = cwd;
            if (arr.Length == 2) dir = await dir.OpenDirectory(arr[1], 0);

            await foreach (var ent in dir.ReadEntries())
            {
                _terminal.Insert(ent.FileName);
                _terminal.InsertNewLine();
            }
        }
        Task CmdNotFound(string[] arr)
        {
            _terminal.Insert("ERROR: command not found");
            _terminal.InsertNewLine();
            return Task.CompletedTask;
        }
    }
}