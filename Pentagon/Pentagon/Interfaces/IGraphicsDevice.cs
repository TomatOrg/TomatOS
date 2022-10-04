using System;
using System.Drawing;

namespace Pentagon.Interfaces;

/// <summary>
/// Represents a single output for the device, basically a monitor 
/// </summary>
public interface IGraphicsOutput
{
    /// <summary>
    /// The width of the output 
    /// </summary>
    int Width { get; }
    
    /// <summary>
    /// The height of the output
    /// </summary>
    int Height { get; }

    /// <summary>
    /// Set the framebuffer to be used for this output, and specifically use
    /// the given rect from the framebuffer.
    /// </summary>
    void SetFramebuffer(IFramebuffer framebuffer, in Rectangle rect);

}

/// <summary>
/// Represents a resource that is related to the GPU, this resource can have
/// a CPU backing buffer that can then be DMAd to the GPU as needed
/// </summary>
public interface IFramebuffer : IDisposable
{
    /// <summary>
    /// The width of the framebuffer
    /// </summary>
    int Width { get; }
    
    /// <summary>
    /// The height of the framebuffer
    /// </summary>
    int Height { get; }
    
    /// <summary>
    /// Allows to attach multiple 
    /// </summary>
    Memory<byte> Backing { set; }
   
    /// <summary>
    /// Blit from the buffer starting at the given offset, to the Rectangle inside
    /// the framebuffer.
    ///
    /// Note: blit could in theory happen only on a flush, which means that until a flush you
    ///       should not modify the backing store of the blit you just made
    /// </summary>
    void Blit(int offset, in Rectangle rectangle);

    /// <summary>
    /// Flushes the framebuffer to all the outputs it is attached to
    ///
    /// TODO: rectangle to blit from the buffer
    /// </summary>
    void Flush();

}

/// <summary>
/// Represents a single graphics device, the device can share the framebuffers and
/// blit them to all the outputs available
/// </summary>
public interface IGraphicsDevice
{

    /// <summary>
    /// The graphics outputs this device has 
    /// </summary>
    public IGraphicsOutput[] Outputs { get; }

    /// <summary>
    /// Creates a GPU resource to store data on 
    /// </summary>
    public IFramebuffer CreateFramebuffer(int width, int height);

}
