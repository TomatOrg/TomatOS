namespace System.Reflection;

[AttributeUsage(AttributeTargets.Assembly, Inherited = false)]
public sealed class AssemblyVersionAttribute : Attribute
{

    public string Version { get; }

    public AssemblyVersionAttribute(string version)
    {
        Version = version;
    }

}