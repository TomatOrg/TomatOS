using System;
using System.Collections.Generic;
using Tomato.Gui.Framework;

namespace Tomato.Gui.Widgets;

public class Column : Widget
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

    public Column(Widget[] children, Align align = Align.Center)
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
        
        //One of the axis are not flexible, need to calculate actual size
        if (totalFlexX == 0 || totalFlexY == 0)
        {
            var childrenSizes = new(Expr, Expr)[_builtChildren.Length];
            for (var i = 0; i < _builtChildren.Length; i++)
            {
                if (childrenFlexX[i] == 0 || childrenFlexY[i] == 0)
                {
                    childrenSizes[i] = _builtChildren[i]
                        .Layout(0, minHeight, Expr.Inf, maxHeight);
                }
                else
                {
                    childrenSizes[i] = (0, 0);
                }
            }

            if (totalFlexX == 0)
            {
                width = 0;
                foreach (var size in childrenSizes)
                {
                    width = width.Max(size.Item1);
                }
            }

            if (totalFlexY == 0)
            {
                height = 0;
                foreach (var size in childrenSizes)
                {
                    height += size.Item2;
                }
            }
        }

        return (width, height);
    }

    public override List<Command> Render(Expr left, Expr top, Expr right, Expr bottom)
    {
        var result = new List<Command>();

        var freeSpace = bottom - top;
        
        var childrenFlexX = new float[_builtChildren.Length];
        var childrenFlexY = new float[_builtChildren.Length];

        var totalFlex = 0.0f;
        for (var i = 0; i < _builtChildren.Length; i++)
        {
            var flexX = _builtChildren[i].FlexX;
            var flexY = _builtChildren[i].FlexY;
            childrenFlexX[i] = flexX;
            childrenFlexY[i] = flexY;
            totalFlex += flexY;
        }

        var childrenSizes = new(Expr, Expr)[_builtChildren.Length];
        for (var i = 0; i < _builtChildren.Length; i++)
        {
            if (childrenFlexX[i] == 0 || childrenFlexY[i] == 0)
            {
                childrenSizes[i] = _builtChildren[i]
                    .Layout(0, 0, right - left, Expr.Inf);
            }
            else
            {
                childrenSizes[i] = (0, 0);
            }

            if (childrenFlexY[i] == 0)
            {
                freeSpace -= childrenSizes[i].Item2;
            }
        }

        // TODO: Support floating point memes
        var perFlexUnitSize = freeSpace.Abs() / (int)totalFlex;
        for (var i = 0; i < childrenFlexY.Length; i++)
        {
            if (childrenFlexY[i] > 0)
            {
                childrenSizes[i].Item2 = perFlexUnitSize * (int)childrenFlexY[i];
            }
        }

        for (var i = 0; i < childrenFlexX.Length; i++)
        {
            if (childrenFlexX[i] > 0)
            {
                childrenSizes[i].Item1 = right - left;
            }
        }

        var currentTopCoord = top;
        for (var i = 0; i < _builtChildren.Length; i++)
        {
            Expr itemLeft, itemRight;
            if (_align == Align.Center)
            {
                itemLeft = left + (right - left - childrenSizes[i].Item1) / 2;
                itemRight = left + (right - left + childrenSizes[i].Item1) / 2;
            }
            else if (_align == Align.Top)
            {
                itemLeft = left;
                itemRight = left + childrenSizes[i].Item1;
            }
            else
            {
                throw new InvalidOperationException();
            }
            
            result.AddRange(_builtChildren[i]
                .Render(itemLeft, currentTopCoord, itemRight, currentTopCoord + childrenSizes[i].Item2)
            );
            currentTopCoord += childrenSizes[i].Item2;
        }

        return result;
    }
}