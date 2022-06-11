namespace System.Reflection;

[AttributeUsage(AttributeTargets.Class | AttributeTargets.Interface | AttributeTargets.Struct)]
public sealed class DefaultMemberAttribute : Attribute
{
    
    public string MemberName { get; }

    public DefaultMemberAttribute(string memberName)
    {
        MemberName = memberName;
    }
    
}