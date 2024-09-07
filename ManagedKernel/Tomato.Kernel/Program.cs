namespace Tomato.Kernel;

class Program
{
    private int a = 123;

    public int Add(int b)
    {
        return a + b;
    }
    
    static int Main(string[] args)
    {
        var p = new Program();
        return p.Add(456);
    }
}