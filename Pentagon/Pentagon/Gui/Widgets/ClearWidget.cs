using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq.Expressions;

namespace Pentagon.Gui.Widgets;

public class ClearWidget : Widget
{
    
    public override bool BuildsChildren => true;

    public override float FlexX => 1.0f;
    public override float FlexY => 1.0f;

    private Widget _builtChild = null;

    public Expression Color { get; }
    public Widget Child { get; }

    public ClearWidget(Widget child, Expression color)
    {
        Color = color;
        Child = child;
    }

    public ClearWidget(Widget child, uint color)
        : this(child, Expression.Constant(color))
    {
    }

    public ClearWidget(Widget child, Color color)
        : this(child, (uint)color.ToArgb())
    {
    }

    public override Widget Build()
    {
        _builtChild = Child.BuildRecursively();
        return this;
    }

    public override (Expression, Expression) Layout(Expression minWidth, Expression minHeight, Expression maxWidth, Expression maxHeight)
    {
        return _builtChild.Layout(
            Expression.Constant(0),
            Expression.Constant(0),
            maxWidth, maxHeight
        );
    }

    public override List<Command> Render(Expression left, Expression top, Expression right, Expression bottom)
    {
        var rendered = _builtChild.Render(left, top, right, bottom);
        rendered.Insert(0, new ClearCommand{ Color = Color});
        return rendered;
    }
}