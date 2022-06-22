using System.Reflection;
using System.Runtime.InteropServices;

namespace System;

[StructLayout(LayoutKind.Sequential)]
public abstract class Delegate
{

    private IntPtr _fnptr;
    private object _target;

}