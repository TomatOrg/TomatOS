using System;

namespace Pentagon;

public class Kernel
{

    struct Vec2
    {
        public int X { get; set; }
        public int Y { get; set; }
    }

    public static int Main()
    {
        var vec = new Vec2
        {
            X = 1,
            Y = 2
        };
        
        return vec.X + vec.Y;
    }
    
}