namespace System.Reflection;

[AttributeUsage(AttributeTargets.Assembly, Inherited = false)]
public sealed class AssemblyConfigurationAttribute : Attribute
{
    
    public string Configuration { get; }

    public AssemblyConfigurationAttribute(string configuration)
    {
        Configuration = configuration;
    }
    
}