using System;
using System.Drawing;
using System.Linq.Expressions;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using Pentagon.DriverServices;
using Pentagon.Graphics;
using Pentagon.Interfaces;

namespace Pentagon.Gui;

/// <summary>
/// This is both a client and server implementation for apps running and displaying locally.
/// </summary>
public class LocalGuiServer : GuiServer
{

    private IFramebuffer _framebuffer;
    private Memory<uint> _memory;
    private int _width, _height;

    public LocalGuiServer(IFramebuffer framebuffer)
    {
        _framebuffer = framebuffer;
        _width = framebuffer.Width;
        _height = framebuffer.Height;
     
        // allocate the framebuffer
        var size = framebuffer.Width * framebuffer.Height * 4;
        var pageCount = KernelUtils.DivideUp(size, MemoryServices.PageSize);
        var memory = MemoryServices.AllocatePages(pageCount).Memory;
        
        // set the backing 
        _framebuffer.Backing = memory;
        
        // keep it as a uint array for blitter
        _memory = MemoryMarshal.Cast<byte, uint>(memory);
    }

    // TODO: add support for compiling expressions, it will
    //       make the code much faster :)
    private int Eval(Expression e)
    {
        switch (e.NodeType)
        {
            case ExpressionType.Constant:
            {
                // TODO: better support for other types
                var node = (ConstantExpression)e;
                return (int)node.Value!;
            } break;

            case ExpressionType.Parameter:
            {
                var node = (ParameterExpression)e;
                // TODO: type checking
                return node.Name switch
                {
                    "width" => _width,
                    "height" => _height,
                    _ => throw new InvalidOperationException($"unknown gui variable {node.Name}")
                };
            } break;
            
            default:
                throw new InvalidOperationException("Invalid gui expression type");
        }
    }

    #region API

    

    #endregion
    
    public override void Accept()
    {
        // nothing to do in here
    }

    public override void Handle()
    {
        // TODO: handle inputs in here and push them to the event queue
        //       for the GetEvent to handle
        while (true) ;
    }

    public override void UpdateScene(Scene scene)
    {
        // TODO: use an oplist instead of a plain expression, that can be 
        //       used to calculate common expressions just once 
        
        foreach (var command in scene.Commands)
        {
            switch (command.CommandType)
            {
                case CommandType.Clear:
                {
                    var cmd = (ClearCommand)command;
                    var blitter = new Blitter(_memory, _width, (uint)Eval(cmd.Color));
                    blitter.BlitRect(0, 0, _framebuffer.Width, _height);
                } break;
                
                case CommandType.Rect:
                {
                    var cmd = (RectCommand)command;
                    var blitter = new Blitter(_memory, _width, (uint)Eval(cmd.Color));
                    var left = Eval(cmd.Left);
                    var top = Eval(cmd.Top);
                    var right = Eval(cmd.Right);
                    var bottom = Eval(cmd.Bottom);
                    Log.LogHex((ulong)(right - left));
                    Log.LogString("\n");
                    Log.LogHex((ulong)(bottom - top));
                    blitter.BlitRect(left, top, unchecked(right - left), unchecked(bottom - top));
                } break;
                
                default:
                    throw new InvalidOperationException("Invalid gui command type");
            }
        }
        
        // blit the backing to the framebuffer
        _framebuffer.Blit(0, new Rectangle(0, 0, _width, _height));
        
        // flush the framebuffer to the output 
        _framebuffer.Flush();
    }

}