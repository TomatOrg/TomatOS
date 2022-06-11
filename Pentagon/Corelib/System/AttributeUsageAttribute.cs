namespace System
{
    public sealed class AttributeUsageAttribute : Attribute
    {

        public AttributeTargets ValidOn { get; set; }
        public bool AllowMultiple { get; set; }
        public bool Inherited { get; set; }
        
        public AttributeUsageAttribute(AttributeTargets validOn)
        {
            ValidOn = validOn;
            Inherited = true;
        }
        
        internal AttributeUsageAttribute(AttributeTargets validOn, bool allowMultiple, bool inherited)
        {
            ValidOn = validOn;
            AllowMultiple = allowMultiple;
            Inherited = inherited;
        }
        
    }
}