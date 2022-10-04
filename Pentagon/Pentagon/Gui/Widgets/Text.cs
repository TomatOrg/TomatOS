using System.Collections.Generic;
using System.Drawing;
using Pentagon.Gui.Framework;

namespace Pentagon.Gui.Widgets;

public class Text : Widget
{
    public override bool BuildsChildren => true;

    private string _text;
    private Expr _color;
    private Expr _fontSize;
    
    public Text(string text, Expr color, Expr fontSize = null)
    {
        _text = text;
        _color = color;
        _fontSize = fontSize ?? 14;
    }
    
    public Text(string text, Color color, Expr fontSize = null)
        : this(text, (uint)color.ToArgb(), fontSize)
    {  
    }
    
    public Text(string text, Expr fontSize = null)
        : this(text, Color.White, fontSize)
    {
    }

    public override Widget Build()
    {
        return this;
    }

    public override (Expr, Expr) Layout(Expr minWidth, Expr minHeight, Expr maxWidth, Expr maxHeight)
    {
        return (Expr.MeasureTextX(_text, _fontSize).Min(maxWidth).Max(minWidth), Expr.MeasureTextY(_text, _fontSize).Min(maxHeight).Max(minHeight));
    }

    public override List<Command> Render(Expr left, Expr top, Expr right, Expr bottom)
    {
        return new List<Command>
        {
            new TextCommand
            {
                Color = _color,
                Text = _text,
                FontSize = _fontSize,
                X = (left + right) / 2,
                Y = (bottom + top) / 2,
            }
        };
    }
}