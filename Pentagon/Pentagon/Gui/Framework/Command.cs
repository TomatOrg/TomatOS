using System.Linq.Expressions;

namespace Pentagon.Gui;

internal enum CommandType
{
    
    Clear,
    Rect,
    // RoundedRect,
    // Reply,
    // SetVar,
    // EventHandler,
    // WatchVar,
    // AckWatch,
    // Text,
    // If,
    // Save,
    // Restore,
    // ClipRect,
    // Image,
    
}

public abstract class Command
{
    
    internal abstract CommandType CommandType { get; }

}

public class ClearCommand : Command
{
    internal override CommandType CommandType => CommandType.Clear;
    
    public Expr Color { get; set; }

}

public class RectCommand : Command
{
    internal override CommandType CommandType => CommandType.Rect;

    public Expr Left { get; set; }
    public Expr Top { get; set; }
    public Expr Right { get; set; }
    public Expr Bottom { get; set; }
    public Expr Color { get; set; }

}
