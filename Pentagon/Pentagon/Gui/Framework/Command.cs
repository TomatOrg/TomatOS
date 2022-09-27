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
    
    public Expression Color { get; set; }

}

public class RectCommand : Command
{
    internal override CommandType CommandType => CommandType.Rect;

    public Expression Left { get; set; }
    public Expression Top { get; set; }
    public Expression Right { get; set; }
    public Expression Bottom { get; set; }
    public Expression Color { get; set; }

}
