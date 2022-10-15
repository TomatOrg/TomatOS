using System.Collections.Generic;
using Tomato.Hal.Interfaces;

namespace Tomato.Hal.Drivers.PlainFramebuffer;

internal class PlainGraphicsDevice : IGraphicsDevice
{

    private PlainGraphicsOutput[] _outputs = null;
    
    /// <summary>
    /// Don't give the user our list
    /// </summary>
    public IEnumerable<IGraphicsOutput> Outputs
    {
        get
        {
            for (var i = 0; i < _outputs.Length; i++)
            {
                yield return _outputs[i];
            }
        }
    }

    public int OutputsCount => _outputs.Length;

    internal PlainGraphicsDevice()
    {
        var index = 0;
        var outputs = new List<PlainGraphicsOutput>();
        while (Hal.GetNextFramebuffer(ref index, out var addr, out var width, out var height, out var pitch))
        {
            var buffer = MemoryServices.Map(addr, pitch * height);
            outputs.Add(new PlainGraphicsOutput(width, height, pitch, buffer));
        }
        _outputs = outputs.ToArray();
    }
    
    public IFramebuffer CreateFramebuffer(int width, int height)
    {
        return new PlainFramebuffer(width, height);
    }
}