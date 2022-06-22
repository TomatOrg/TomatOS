using System.Runtime.InteropServices;

namespace System;

[StructLayout(LayoutKind.Sequential)]
public abstract class MulticastDelegate : Delegate
{

    private MulticastDelegate _next;

}