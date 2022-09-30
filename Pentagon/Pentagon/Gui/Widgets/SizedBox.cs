using System.Collections.Generic;
using Pentagon.Gui.Framework;

namespace Pentagon.Gui.Widgets;

public class SizedBox : Widget
{
    public override bool BuildsChildren => true;

    private Widget _child;
    private Widget _builtChild;
    private Expr _width;
    private Expr _height;

    public override float FlexX => _width is null ? 0 : 1;
    public override float FlexY => _height is null ? 0 : 1;

    public SizedBox(Widget child, Expr width=null, Expr height=null)
    {
        _child = child;
        _width = width;
        _height = height;
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
        var width = _width ?? maxWidth;
        var height = _height ?? maxHeight;
        return (width, height);
    }

    public override List<Command> Render(Expr left, Expr top, Expr right, Expr bottom)
    {
        return _builtChild != null ? _builtChild.Render(left, top, right, bottom) : new List<Command>();
    }
}