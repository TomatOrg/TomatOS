using System.Collections.Generic;

namespace Pentagon.Gui.Widgets;

public class Flexible : Widget
{
    public override bool BuildsChildren => true;

    public override float FlexX { get; }
    public override float FlexY { get; }

    private Widget _child;
    private Widget _builtChild = null;

    public Flexible(float flexX = 1.0f, float flexY = 1.0f, Widget child = null)
    {
        _child = child;
        FlexX = flexX;
        FlexY = flexY;
    }
    
    public override Widget Build()
    {
        if (_child != null)
        {
            _builtChild = _child.BuildRecursively();
        }

        return this;
    }

    public override (Expr, Expr) Layout(Expr minWidth, Expr minHeight, Expr maxWidth, Expr maxHeight)
    {
        var width = maxWidth;
        var height = maxHeight;
        
        if (_builtChild != null)
        {
            var layout = _builtChild.Layout(minWidth, minHeight, maxWidth, maxHeight);
            if (FlexX == 0)
                width = layout.Item1;
            if (FlexY == 0)
                height = layout.Item2;
        }

        return (width, height);
    }

    public override List<Command> Render(Expr left, Expr top, Expr right, Expr bottom)
    {
        return _builtChild != null ? _builtChild.Render(left, top, right, bottom) : new List<Command>();
    }
}