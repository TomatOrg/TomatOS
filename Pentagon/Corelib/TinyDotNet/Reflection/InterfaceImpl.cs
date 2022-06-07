using System;
using System.Reflection;
using System.Runtime.InteropServices;

namespace TinyDotNet.Reflection;

[StructLayout(LayoutKind.Sequential)]
internal class InterfaceImpl
{

    private Type _interfaceType;
    private int _vtableOffset;

}