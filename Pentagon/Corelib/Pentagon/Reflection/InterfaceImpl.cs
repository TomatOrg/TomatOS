using System;
using System.Reflection;
using System.Runtime.InteropServices;

namespace Pentagon.Reflection;

[StructLayout(LayoutKind.Sequential)]
public class InterfaceImpl
{

    private Type _interfaceType;
    private int _vtableOffset;

}