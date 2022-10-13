using System;
using System.Collections.Generic;
using Tomato.Gui.Framework;

namespace Tomato.Gui.Widgets;

public enum StackFit
{
    Expand,
    Tight,
}

public class Stack : Widget
{
    public override bool BuildsChildren => true;

    private Widget[] _children;
    private Widget[] _builtChildren;
    private Align _align;
    private StackFit _fit;

    public override float FlexX
    {
        get
        {
            if (_fit != StackFit.Expand) 
                return 0;
            
            foreach (var child in _children)
                if (child.FlexX != 0.0f)
                    return 1;

            return 0;

        }
    }

    public override float FlexY
    {
        get
        {
            if (_fit != StackFit.Expand) 
                return 0;
            
            foreach (var child in _children)
                if (child.FlexY != 0.0f)
                    return 1;

            return 0;
        }
    }
    
    public Stack(Widget[] children, Align align = Align.Center, StackFit fit = StackFit.Expand)
    {
        _children = children;
        _builtChildren = new Widget[_children.Length];
        _align = align;
        _fit = fit;
    }

    public override Widget Build()
    {
        for (var i = 0; i < _children.Length; i++)
        {
            _builtChildren[i] = _children[i].BuildRecursively();
        }

        return this;
    }

    public override (Expr, Expr) Layout(Expr minWidth, Expr minHeight, Expr maxWidth, Expr maxHeight)
    {
        var childrenFlexX = new float[_builtChildren.Length];
        var childrenFlexY = new float[_builtChildren.Length];
        
        var totalFlexX = 0.0f;
        var totalFlexY = 0.0f;
        for (var i = 0; i < _builtChildren.Length; i++)
        {
            var flexX = _builtChildren[i].FlexX;
            var flexY = _builtChildren[i].FlexY;
            childrenFlexX[i] = flexX;
            childrenFlexY[i] = flexY;
            totalFlexX += flexX;
            totalFlexY += flexY;
        }

        Expr width, height;
        if (_fit == StackFit.Expand)
        {
            width = maxWidth;
            height = maxHeight;
        }
        else
        {
            width = 0;
            height = 0;
        }

        // One of the axis are not flexible, need to calculate actual size
        if (totalFlexX == 0 || totalFlexY == 0 || _fit == StackFit.Tight)
        {
            var childrenSizes = new(Expr, Expr)[_builtChildren.Length];
            for (var i = 0; i < _builtChildren.Length; i++)
            {
                if (childrenFlexX[i] == 0 || childrenFlexY[i] == 0)
                {
                    childrenSizes[i] = _builtChildren[i]
                        .Layout(0, 0, Expr.Inf, Expr.Inf);
                }
                else
                {
                    childrenSizes[i] = (0, 0);
                }
            }
            
            if (_builtChildren.Length != 0 && (totalFlexX == 0 || _fit == StackFit.Tight))
            {
                foreach (var childSize in childrenSizes)
                {
                    width = width.Max(childSize.Item1);
                }
            } else if (_builtChildren.Length == 0)
            {
                width = 0;
            }
            
            if (_builtChildren.Length != 0 && (totalFlexY == 0 || _fit == StackFit.Tight))
            {
                foreach (var childSize in childrenSizes)
                {
                    width = width.Max(childSize.Item2);
                }
            } else if (_builtChildren.Length == 0)
            {
                width = 0;
            }
        }
        else
        {
            foreach (var child in _builtChildren)
            {
                child.Layout(minWidth, minHeight, width, height);
            }
        }

        return (width, height);
    }

    public override List<Command> Render(Expr left, Expr top, Expr right, Expr bottom)
    {
        var result = new List<Command>();

        var childrenFlexX = new float[_builtChildren.Length];
        var childrenFlexY = new float[_builtChildren.Length];
        for (var i = 0; i < _builtChildren.Length; i++)
        {
            childrenFlexX[i] = _builtChildren[i].FlexX;
            childrenFlexY[i] = _builtChildren[i].FlexY;
        }
        
        var childrenSizes = new(Expr, Expr)[_builtChildren.Length];
        for (var i = 0; i < _builtChildren.Length; i++)
        {
            if (childrenFlexX[i] == 0 || childrenFlexY[i] == 0)
            {
                childrenSizes[i] = _builtChildren[i]
                    .Layout(0, 0, Expr.Inf, Expr.Inf);
            }
            else
            {
                childrenSizes[i] = (0, 0);
            }
        }

        for (var i = 0; i < childrenFlexY.Length; i++)
            if (childrenFlexY[i] > 0)
                childrenSizes[i].Item2 = bottom - top;

        for (var i = 0; i < childrenFlexX.Length; i++)
            if (childrenFlexX[i] > 0)
                childrenSizes[i].Item1 = right - left;

        for (var i = 0; i < _builtChildren.Length; i++)
        {
            var child = _builtChildren[i];
            var childSize = childrenSizes[i];
            if (_align == Align.Center)
            {
                var childLeft = (right + left - childSize.Item1) / 2;
                var childTop = (bottom + top - childSize.Item2) / 2;
                var childRight = (right + left + childSize.Item1) / 2;
                var childBottom = (bottom + top + childSize.Item2) / 2;
                result.AddRange(child.Render(childLeft, childTop, childRight, childBottom));
            }
            else if (_align == Align.Top)
            {
                result.AddRange(child.Render(left, top, left + childSize.Item1, top + childSize.Item2));
            }
            else
            {
                throw new InvalidOperationException();
            }
        }
        
        return result;
    }
}