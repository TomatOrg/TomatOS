namespace System.Reflection;

[AttributeUsage(AttributeTargets.Assembly, Inherited = false)]
public sealed class AssemblyTitleAttribute : Attribute
{
    
    public string Title { get; }

    public AssemblyTitleAttribute(string title)
    {
        Title = title;
    }
    
}