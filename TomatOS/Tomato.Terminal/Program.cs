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
    private static FileSystemManager _fsManager;
    private static Terminal _terminal;

    private static IKeyboard _keyboard;
    private static IGraphicsDevice _device;
    private static IFileSystem _fileSystem;
    
    private static void NewKeyboard(IKeyboard keyboard)
    {
        if (_keyboard != null)
            return;
        _keyboard = keyboard;
    }

    private static void NewGraphicsDevice(IGraphicsDevice device)
    {
        if (_device != null)
            return;
        _device = device;
    }

    private static void NewFileSystem(IFileSystem fs)
    {
        if (_fileSystem != null)
            return;
        _fileSystem = fs;
    }
    
    public static void Main()
    {
        try
        {
            _displayManager = DisplayManager.Claim();
            lock (_displayManager)
            {
                _displayManager.NewKeyboardCallback = NewKeyboard;
                _displayManager.NewGraphicsDeviceCallback = NewGraphicsDevice;
            }

            _fsManager = FileSystemManager.Claim();
            lock (_fsManager)
            {
                _fsManager.NewFileSystemCallback = NewFileSystem;
            }
        }
        catch (InvalidOperationException)
        {
            return;
        }
        
        var output = _device.Outputs[0];

        // create a gpu framebuffer and attach it to the first output
        var framebuffer = _device.CreateFramebuffer(output.Width, output.Height);
        output.SetFramebuffer(framebuffer, new Rectangle(0, 0, output.Width, output.Height));

        // allocate a cpu backing 
        var m = new byte[framebuffer.Width * framebuffer.Height * 4].AsMemory();
        var memory = MemoryMarshal.Cast<byte, uint>(m);
        framebuffer.Backing = m;

        _terminal = new Terminal(framebuffer, memory, _keyboard, new Font(Typeface.Default, 16));
        _terminal.Insert("Welcome to TomatOS!");
        _terminal.InsertNewLine();

        Task.Run(Shell);
    }

    private static async Task Shell()
    {
        var root = _fileSystem.OpenVolume().Result;
        _terminal.Insert($"Loaded rootfs with label \"{_fileSystem.VolumeLabel}\".");
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
                case "ls": await CmdLs(commandArr); break;
                case "cd": await CmdCd(commandArr); break;
                case "cat": await CmdCat(commandArr); break;
                default: await CmdNotFound(commandArr); break;
            }
        }
        
        async Task<IDirectory> OpenDirFromString(string s)
        {
            var names = s.Split('/');
            var dir = cwd;

            int start = 0, end = names.Length - 1;
            if (s[0] == '/') { dir = root; start++; }

            for (int i = start; i < end; i++)
                dir = await dir.OpenDirectory(names[i], 0);

            if (names[end].Length != 0) dir = await dir.OpenDirectory(names[end], 0);

            return dir;
        }

        async Task<IFile> OpenFileFromString(string s)
        {
            var names = s.Split('/');
            var dir = cwd;

            int start = 0, end = names.Length - 1;
            if (s[0] == '/') { dir = root; start++; }

            for (int i = start; i < end; i++)
                dir = await dir.OpenDirectory(names[i], 0);
            
            // if (names[end].Length == 0) fail
            var file = await dir.OpenFile(names[end], 0);
            return file;
        }

        async Task CmdLs(string[] arr)
        {
            if (arr.Length > 2)
            {
                _terminal.Insert("ERROR: wrong syntax for \"ls\".");
                _terminal.InsertNewLine();
                return;
            }
            var dir = cwd;
            if (arr.Length == 2) dir = await OpenDirFromString(arr[1]);

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
            var file = await OpenFileFromString(arr[1]);
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