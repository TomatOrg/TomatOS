namespace Pentagon;

public class Test
{


    class A<T>
    {
        private T _lol;
    }

    class B<T>
    {
        private T _lol;
    }

    public static void Main()
    {
        var tA = typeof(A<>);
        var tB = typeof(A<int>);
        Console.WriteLine("{} {}", tA, tB);
    }
    
    
}