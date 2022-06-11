namespace System.Runtime.CompilerServices;

[AttributeUsage(AttributeTargets.Property)]
public sealed class IndexerNameAttribute : Attribute
{

    public IndexerNameAttribute(string indexerName)
    {
    }
    
}