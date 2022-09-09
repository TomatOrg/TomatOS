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

    public enum PointMode
    {
        Points,
        Lines,
        Polygon
    }
    
    public struct Paint
    {
        
        /// <summary>
        /// Returns true if paint prevents all drawing
        /// </summary>
        public bool NothingToDraw { get; set; }
    
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
        /// Returns the requested width of strokes
        /// </summary>
        public float StrokeWidth { get; set; }
        
    }

    public void FillPaint(in Paint paint);
    public void DrawPoints(PointMode mode, in Paint paint, params PointF[] pts);
    public void DrawPoint(SizeF p, in Paint paint);
    public void DrawLine(SizeF x0, SizeF x1, in Paint paint);
    public void DrawRect(RectangleF rect, in Paint paint);
    public void DrawOval(RectangleF rect, in Paint paint);
    public void DrawCircle(PointF center, float radius, in Paint paint);
    public void DrawArc(RectangleF oval, float startAngle, float sweepAngle, bool useCenter, in Paint paint);
    public void DrawRoundRect(RectangleF rect, float rx, float ry, in Paint paint);
    public void DrawGlyph(int codePoint, float x, float y, in Paint paint);

}