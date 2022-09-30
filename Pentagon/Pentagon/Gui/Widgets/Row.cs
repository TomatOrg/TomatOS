using System;
using System.Collections.Generic;
using Pentagon.Gui.Framework;

namespace Pentagon.Gui.Widgets;

public class Row : Widget
{
    public override bool BuildsChildren => true;

    private Widget[] _children;
    private Widget[] _builtChildren;
    private Align _align;

    public override float FlexX
    {
        get
        {
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
            foreach (var child in _children)
                if (child.FlexY != 0.0f)
                    return 1;

            return 0;
        }
    }

    public Row(Widget[] children, Align align = Align.Center)
    {
        _children = children;
        _align = align;
    }

    public override Widget Build()
    {
        _builtChildren = new Widget[_children.Length];
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

        var width = maxWidth;
        var height = maxHeight;
        
        // One of the axis are not flexible, need to calculate actual size
        var childrenSizes = new(Expr, Expr)[_builtChildren.Length];
        for (var i = 0; i < _builtChildren.Length; i++)
        {
            var child = _builtChildren[i];
            var childFlexX = childrenFlexX[i];
            var childFlexY = childrenFlexY[i];
            
            childrenSizes[i] = child.Layout(0, minHeight, Expr.Inf, maxHeight);

            if (childFlexX != 0)
            {
                childrenSizes[i].Item1 = 0;
            }

            if (childFlexY != 0)
            {
                childrenSizes[i].Item2 = 0;
            }
        }

        if (totalFlexX == 0)
        {
            width = 0;
            foreach (var size in childrenSizes)
            {
                width += size.Item1;
            }
        }

        if (totalFlexY == 0)
        {
            height = 0;
            foreach (var size in childrenSizes)
            {
                height = height.Max(size.Item2);
            }
        }

        return (width, height);
    }

    public override List<Command> Render(Expr left, Expr top, Expr right, Expr bottom)
    {
        var result = new List<Command>();

        var freeSpace = right - left;
        
        var childrenFlexX = new float[_builtChildren.Length];
        var childrenFlexY = new float[_builtChildren.Length];

        var totalFlex = 0.0f;
        for (var i = 0; i < _builtChildren.Length; i++)
        {
            var flexX = _builtChildren[i].FlexX;
            var flexY = _builtChildren[i].FlexY;
            childrenFlexX[i] = flexX;
            childrenFlexY[i] = flexY;
            totalFlex += flexX;
        }

        var childrenSizes = new(Expr, Expr)[_builtChildren.Length];
        for (var i = 0; i < _builtChildren.Length; i++)
        {
            var child = _builtChildren[i];
            var childFlexX = childrenFlexX[i];
            var childFlexY = childrenFlexY[i];

            if (childFlexX == 0 || childFlexY == 0)
            {
                childrenSizes[i] = child.Layout(0, 0, Expr.Inf, bottom - top);
                
                if (childFlexX == 0)
                {
                    freeSpace -= childrenSizes[i].Item1;
                }
            }
            else
            {
                childrenSizes[i] = (0, 0);
            }
        }

        // TODO: Support floating point memes
        var perFlexUnitSize = freeSpace.Abs() / (int)totalFlex;
        for (var i = 0; i < childrenFlexX.Length; i++)
        {
            var childFlexX = childrenFlexX[i];
            
            if (childFlexX > 0)
            {
                childrenSizes[i].Item1 = perFlexUnitSize * (int)childFlexX;
            }
        }

        for (var i = 0; i < childrenFlexY.Length; i++)
        {
            var childFlexY = childrenFlexY[i];
            
            if (childFlexY > 0)
            {
                childrenSizes[i].Item2 = bottom - top;
            }
        }

        var currentLeftCoord = left;
        for (var i = 0; i < _builtChildren.Length; i++)
        {
            var child = _builtChildren[i];
            var childSize = childrenSizes[i];
            
            Expr itemTop, itemBottom;
            if (_align == Align.Center)
            {
                itemTop = top + (bottom - top - childSize.Item2) / 2;
                itemBottom = top + (bottom - top + childSize.Item2) / 2;
            }
            else if (_align == Align.Top)
            {
                itemTop = top;
                itemBottom = top + childSize.Item2;
            }
            else
            {
                throw new InvalidOperationException();
            }
            
            result.AddRange(child
                .Render(currentLeftCoord, itemTop, currentLeftCoord + childSize.Item1, itemBottom)
            );
            currentLeftCoord += childSize.Item1;
        }

        return result;
    }
}