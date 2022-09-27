using System;
using System.Collections.Generic;
using System.Linq.Expressions;

namespace Pentagon.Gui;

public abstract class Widget
{

    public virtual bool BuildsChildren => false;

    public virtual float FlexX => 0.0f;
    public virtual float FlexY => 0.0f;

    public virtual (Expression, Expression) Layout(Expression minWidth, Expression minHeight, Expression maxWidth, Expression maxHeight)
    {
        throw new InvalidOperationException("This widget shouldn't be rendered directly");
    }
    
    public virtual List<Command> Render(Expression left, Expression top, Expression right, Expression bottom)
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