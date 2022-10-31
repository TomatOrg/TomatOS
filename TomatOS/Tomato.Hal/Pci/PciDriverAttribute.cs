using System;

namespace Tomato.Hal.Pci;

[AttributeUsage(AttributeTargets.Class, AllowMultiple = true)]
public class PciDriverAttribute : Attribute
{
    public ushort VendorId { get; }
    public ushort DeviceId { get; }
    
    public byte ClassCode { get; }
    public byte Subclass { get; }
    public byte ProgIf { get; }

    public PciDriverAttribute(ushort vendorId, ushort deviceId)
    {
        VendorId = vendorId;
        DeviceId = deviceId;
    }

    public PciDriverAttribute(byte classCode, byte subclass, byte progIf)
    {
        VendorId = 0xFFFF;
        ClassCode = classCode;
        Subclass = subclass;
        ProgIf = progIf;
    }
}