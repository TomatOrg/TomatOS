using System.Collections.Generic;
using System.Linq.Expressions;
using Pentagon.DriverServices;
using Pentagon.Gui.Framework;

namespace Pentagon.Gui.Widgets;

public class Padding : Widget
{
    public override bool BuildsChildren => true;

    private Widget _child;
    private Widget _builtChild;
    
    private Expr _left;
    private Expr _top;
    private Expr _right;
    private Expr _bottom;

    public override float FlexX
    {
        get
        {
            if (_builtChild != null)
            {
                return _builtChild.FlexX;
            }
            return 0;
        }
    }

    public override float FlexY
    {
        get
        {
            if (_builtChild != null)
            {
                return _builtChild.FlexY;
            }
            return 0;
        }
    }

    public Padding(
        Widget child, 
        Expr left = null, Expr top = null, 
        Expr right = null, Expr bottom = null,
        Expr horizontal = null, Expr vertical = null, Expr all = null
    )
    {
        _child = child;
        
        if (all is not null)
        {
            _left = all;
            _top = all;
            _right = all;
            _bottom = all;
        }
        else
        {
            _left = left ?? 0;
            _top = top ?? 0;
            _right = right ?? 0;
            _bottom = bottom ?? 0;
        }

        if (horizontal is not null)
        {
            _left = horizontal;
            _right = horizontal;
        }

        if (vertical is not null)
        {
            _top = vertical;
            _bottom = vertical;
        }
    }

    public override Widget Build()
    {
        _builtChild = _child.BuildRecursively();
        return this;
    }

    public override (Expr, Expr) Layout(Expr minWidth, Expr minHeight, Expr maxWidth, Expr maxHeight)
    {
        var childMinWidth = minWidth - _left - _right;
        var childMinHeight = minHeight - _top - _bottom;
        var childMaxWidth = maxWidth - _left - _right;
        var childMaxHeight = maxHeight - _top - _bottom;
        var (childWidth, childHeight) =
            _builtChild.Layout(childMinWidth, childMinHeight, childMaxWidth, childMaxHeight);
        return (childWidth + _left + _right, childHeight + _top + _bottom);
    }
    
    public override List<Command> Render(Expr left, Expr top, Expr right, Expr bottom)
    {
        return _builtChild.Render(left + _left, top + _top, right - _right, bottom - _bottom);
    }
    
}