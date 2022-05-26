using System;
using System.Reflection;

namespace Pentagon.Reflection;

public class InterfaceImpl
{

    private Type _interfaceType;
    private MethodInfo[] _methods;
    private unsafe void* _vtable;

}