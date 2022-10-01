using System;
using System.Collections.Generic;
using Pentagon.Gui.Framework;

namespace Pentagon.Gui;

public abstract class Widget
{

    public virtual bool BuildsChildren => false;

    public virtual float FlexX => 0.0f;
    public virtual float FlexY => 0.0f;

    public virtual (Expr, Expr) Layout(Expr minWidth, Expr minHeight, Expr maxWidth, Expr maxHeight)
    {
        throw new InvalidOperationException("This widget shouldn't be rendered directly");
    }
    
    public virtual List<Command> Render(Expr left, Expr top, Expr right, Expr bottom)
    {
        throw new InvalidOperationException("This widget shouldn't be rendered directly");
    }
    
    public abstract Widget Build();

    public Widget BuildRecursively()
    {
        if (BuildsChildren)
        {
            return Build();
        }
        else
        {
            var built = Build();
            while (!built.BuildsChildren)
            {
                built = built.Build();
            }
            return built.Build();
        }
    }

}