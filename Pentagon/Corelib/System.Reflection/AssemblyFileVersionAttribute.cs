namespace System.Reflection;

[AttributeUsage(AttributeTargets.Assembly, Inherited = false)]
public sealed class AssemblyFileVersionAttribute : Attribute
{
    
    public string Version { get; }

    public AssemblyFileVersionAttribute(string version)
    {
        Version = version;
    }
    
}