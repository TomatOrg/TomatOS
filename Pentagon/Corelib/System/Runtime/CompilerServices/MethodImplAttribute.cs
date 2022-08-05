namespace System.Runtime.CompilerServices;

/// <summary>
/// Specifies the details of how a method is implemented. This class cannot be inherited.
/// </summary>
[AttributeUsage(AttributeTargets.Constructor | AttributeTargets.Method, Inherited=false)]
public sealed class MethodImplAttribute : Attribute
{

    /// <summary>
    /// Gets the MethodImplOptions value describing the attributed method.
    /// </summary>
    public MethodImplOptions Value { get; }
    public MethodCodeType MethodCodeType;
    
    public MethodImplAttribute()
    {
    }

    public MethodImplAttribute(short value)
    {
        Value = (MethodImplOptions)value;
    }

    public MethodImplAttribute(MethodImplOptions methodImplOptions)
    {
        Value = methodImplOptions;
    }
    
}