using System.Collections.Generic;
using System.Drawing;
using System.Linq.Expressions;

namespace Pentagon.Gui.Widgets;

public class Rectangle : Widget
{

    public override bool BuildsChildren => true;

    public override float FlexX => 1.0f;
    public override float FlexY => 1.0f;

    public Expr Color { get; set; }

    public Rectangle(Expr color)
    {
        Color = color;
    }

    public Rectangle(Color color)
        : this((uint)color.ToArgb())
    {
    }
    
    public override Widget Build()
    {
        return this;
    }

    public override (Expr, Expr) Layout(Expr minWidth, Expr minHeight, Expr maxWidth, Expr maxHeight)
    {
        return (maxWidth, maxHeight);
    }

    public override List<Command> Render(Expr left, Expr top, Expr right, Expr bottom)
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