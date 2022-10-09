// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

namespace System.Collections.Generic;

public class KeyNotFoundException : SystemException
{

    public KeyNotFoundException()
        : base("The given key was not present in the dictionary.")
    {
    }

    public KeyNotFoundException(string? message)
        : base(message)
    {
    }

    public KeyNotFoundException(string? message, Exception? innerException)
        : base(message, innerException)
    {
    }
    
}