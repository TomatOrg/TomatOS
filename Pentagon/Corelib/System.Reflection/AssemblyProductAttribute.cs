namespace System.Reflection;

[AttributeUsage(AttributeTargets.Assembly, Inherited = false)]
public sealed class AssemblyProductAttribute : Attribute
{
    
    public string Product { get; }

    public AssemblyProductAttribute(string product)
    {
        Product = product;
    }
    
}