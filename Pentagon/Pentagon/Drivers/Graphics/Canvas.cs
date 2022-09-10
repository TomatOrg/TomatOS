using System.Drawing;
using System.Runtime.CompilerServices;
using Pentagon.Interfaces;

namespace Pentagon.Drivers.Graphics;

public class Canvas
{

    private ICanvas _canvas;

    public Canvas(ICanvas canvas)
    {
        _canvas = canvas;
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public void Clear(Color color)
    {
        Clear((uint)color.ToArgb());
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public void Clear(uint color)
    {
        _canvas.Clear(color);        
    }
    
}