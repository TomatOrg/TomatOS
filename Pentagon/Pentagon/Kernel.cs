using System;

namespace Pentagon;

public class Kernel
{

    struct Vec2
    {
        public int X;
        public int Y;

        public Vec2(int x, int y)
        {
            X = x;
            Y = y;
        }

        public Vec2(int scalar)
        {
            X = scalar;
            Y = scalar;
        }
    }

    public static int Main()
    {
        var first = new Vec2(2, 3);
        Vec2 second = first;
        return first.X;
    }
    
}