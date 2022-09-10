using System;
using System.Drawing;

namespace Pentagon.Interfaces;

public interface ICanvas
{
    
    public enum PaintStyle
    {
        Fill,
        Stroke,
        StrokeAndFill
    }

    public enum PaintCap
    {
        Butt,
        Round,
        Square,
    }

    public struct Paint
    {
        
        /// <summary>
        /// Returns the color that should be used to draw
        /// </summary>
        public uint Color { get; set; }
        
        /// <summary>
        /// Returns true if anti-aliasing should be used when possible
        /// </summary>
        public bool AntiAlias { get; set; }
        
        /// <summary>
        /// Returns the requested style of drawing 
        /// </summary>
        public PaintStyle Style { get; set; }
        
        /// <summary>
        /// Returns the geometry drawn at the beginning and end of strokes.
        /// </summary>
        public PaintCap StrokeCap { get; set; }
        
        /// <summary>
        /// Returns the requested width of strokes
        /// </summary>
        public float StrokeWidth { get; set; }
        
    }

    /// <summary>
    /// 
    /// </summary>
    public void Clear(uint color);
    
    /// <summary>
    /// 
    /// </summary>
    public void DrawLine(PointF x0, PointF x1, in Paint paint);
    
    /// <summary>
    /// 
    /// </summary>
    public void DrawRect(RectangleF rect, in Paint paint);
    
    /// <summary>
    /// 
    /// </summary>
    public void DrawCircle(PointF center, float radius, in Paint paint);
    
    /// <summary>
    /// 
    /// </summary>
    public void DrawRoundRect(RectangleF rect, float rx, float ry, in Paint paint);
    
    /// <summary>
    /// 
    /// </summary>
    public void DrawGlyph(int codePoint, float x, float y, in Paint paint);

}