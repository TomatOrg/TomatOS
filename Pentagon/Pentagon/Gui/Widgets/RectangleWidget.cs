using System.Collections.Generic;
using System.Drawing;
using System.Linq.Expressions;

namespace Pentagon.Gui.Widgets;

public class RectangleWidget : Widget
{

    public override bool BuildsChildren => true;

    public override float FlexX => 1.0f;
    public override float FlexY => 1.0f;

    public Expression Color { get; set; }

    public RectangleWidget(Expression color)
    {
        Color = color;
    }

    public RectangleWidget(uint color)
        : this(Expression.Constant(color))
    {
    }

    public RectangleWidget(Color color)
        : this((uint)color.ToArgb())
    {
    }
    
    public override Widget Build()
    {
        return this;
    }

    public override (Expression, Expression) Layout(Expression minWidth, Expression minHeight, Expression maxWidth, Expression maxHeight)
    {
        return (maxWidth, maxHeight);
    }

    public override List<Command> Render(Expression left, Expression top, Expression right, Expression bottom)
    {
        return new List<Command>
        {
            new RectCommand
            {
                Left = left,
                Top = top,
                Right = right,
                Bottom = bottom,
                
                Color = Color
            }
        };
    }
}