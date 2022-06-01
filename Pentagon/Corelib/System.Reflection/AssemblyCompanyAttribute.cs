namespace System.Reflection;

[AttributeUsage(AttributeTargets.Assembly, Inherited = false)]
public sealed class AssemblyCompanyAttribute : Attribute 
{

    public string Company { get; }
    
    public AssemblyCompanyAttribute(string company)
    {
        Company = company;
    }
    
}