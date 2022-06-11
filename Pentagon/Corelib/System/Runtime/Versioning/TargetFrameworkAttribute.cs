namespace System.Runtime.Versioning;

[AttributeUsage(AttributeTargets.Assembly)]
public sealed class TargetFrameworkAttribute : Attribute
{
    
    public string FrameworkDisplayName { get; set; }
    public string FrameworkName { get; }

    public TargetFrameworkAttribute(string frameworkName)
    {
        FrameworkName = frameworkName;
    }
    
}