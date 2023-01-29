using System;
using System.Diagnostics;
using Tomato.Hal.Platform.Pc;

namespace Tomato.Hal.Acpi.Resource;

public class IoPort
{

    private ushort _port;

    internal IoPort(ushort port)
    {
        _port = port;
    }

    public byte ReadByte()
    {
        return IoPorts.In8(_port);
    }

    public void WriteByte(byte value)
    {
        IoPorts.Out8(_port, value);
    }

}

public class IoResource
{
    
    /// <summary>
    /// The port this device is configured to
    /// </summary>
    public ushort Port { get; }
    
    /// <summary>
    /// The number of contigous I/O ports requested
    /// </summary>
    public byte Length { get; }

    public IoPort this[byte a]
    {
        get
        {
            if (a >= Length)
                throw new IndexOutOfRangeException();
            return new IoPort((ushort)(Port + a));
        }
    }

    internal IoResource(ushort port, byte length)
    {
        Port = port;
        Length = length;
    }
    
}
