namespace System.Buffers;

public class Action
{
    
    public delegate void SpanAction<T, in TArg>(Span<T> span, TArg arg);
    public delegate void ReadOnlySpanAction<T, in TArg>(ReadOnlySpan<T> span, TArg arg);

    internal delegate TResult SpanFunc<TSpan, in T1, in T2, in T3, out TResult>(Span<TSpan> span, T1 arg1, T2 arg2, T3 arg3);
    
}