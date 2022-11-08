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

    private static async Task Shell()
    {
        var fsm = FileSystemManager.Claim();
        fsm.NewFileSystem.WaitOne();
        var fs = fsm.FileSystems[0];

        var root = fs.OpenVolume().Result;
        _terminal.Insert($"Loaded rootfs with label \"{fs.VolumeLabel}\".");
        _terminal.InsertNewLine();

        var path = new List<IDirectory>();
        var cwd = root;

        while (true)
        {
            foreach (var elem in path)
            {
                _terminal.Insert("/");
                _terminal.Insert(elem.FileName);
            }
            _terminal.Insert("> ");

            var command = _terminal.ReadLine();
            var commandArr = command.Split(' ');
            switch (commandArr[0])
            {
                case "ls": await CmdList(commandArr); break;
                case "cd": await CmdCd(commandArr); break;
                case "cat": await CmdCat(commandArr); break;
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

        async Task CmdCd(string[] arr)
        {
            if (arr.Length != 2)
            {
                _terminal.Insert("ERROR: wrong syntax for \"cd\".");
                _terminal.InsertNewLine();
                return;
            }
            var name = arr[1];
            if (name == "..")
            {
                if (path.Count > 0) path.RemoveAt(path.Count - 1);
                cwd = (path.Count == 0) ? root : path[path.Count - 1];
            }
            else
            {
                cwd = await cwd.OpenDirectory(name, 0);
                path.Add(cwd);
            }
        }

        async Task CmdCat(string[] arr)
        {
            if (arr.Length != 2)
            {
                _terminal.Insert("ERROR: wrong syntax for \"cat\".");
                _terminal.InsertNewLine();
                return;
            }
            var file = await cwd.OpenFile(arr[1], 0);
            var d = new byte[512];
            var m = new Memory<byte>(d);
            while (true)
            {
                var br = await file.Read(0, m);
                for (int i = 0; i < br; i++)
                {
                    char c = (char)m.Span[i];
                    if (c == '\n') _terminal.InsertNewLine();
                    else _terminal.Insert($"{c}");
                }
                if (br != 512) break;
            }
            _terminal.InsertNewLine();
        }
        Task CmdNotFound(string[] arr)
        {
            _terminal.Insert("ERROR: command not found");
            _terminal.InsertNewLine();
            return Task.CompletedTask;
        }
    }
}