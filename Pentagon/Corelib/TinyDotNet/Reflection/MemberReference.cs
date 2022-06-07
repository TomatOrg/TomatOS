using System.Runtime.InteropServices;

namespace TinyDotNet.Reflection;

[StructLayout(LayoutKind.Sequential)]
internal class MemberReference
{

    private string _name;
    private int _class;
    private byte[] _signature;

}