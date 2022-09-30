using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq.Expressions;

namespace Pentagon.Gui.Widgets;

public class Clear : Widget
{
    
    public override bool BuildsChildren => true;

    public override float FlexX => 1.0f;
    public override float FlexY => 1.0f;

    private Widget _builtChild = null;

    public Expr Color { get; }
    public Widget Child { get; }

    public Clear(Widget child, Expr color)
    {
        Color = color;
        Child = child;
    }

    public Clear(Widget child, Color color)
        : this(child, (uint)color.ToArgb())
    {
    }

    public override Widget Build()
    {
        _builtChild = Child.BuildRecursively();
        return this;
    }

    public override (Expr, Expr) Layout(Expr minWidth, Expr minHeight, Expr maxWidth, Expr maxHeight)
    {
        if (minWidth is null)
            throw new ArgumentNullException();
        if (minHeight is null)
            throw new ArgumentNullException();
        return _builtChild.Layout(0, 0, maxWidth, maxHeight);
    }

    public override List<Command> Render(Expr left, Expr top, Expr right, Expr bottom)
    {
        var rendered = _builtChild.Render(left, top, right, bottom);
        rendered.Insert(0, new ClearCommand{ Color = Color });
        return rendered;
    }
}