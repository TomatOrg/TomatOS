using System;
using System.Reflection;
using System.Runtime.InteropServices;

namespace TinyDotNet.Reflection;

[StructLayout(LayoutKind.Sequential)]
internal class InterfaceImpl
{

    internal Type _interfaceType;
    internal int _vtableOffset;

}