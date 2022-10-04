// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System.Diagnostics;
using System.Runtime.CompilerServices;

namespace System;

internal static partial class SpanHelpers
{
    
    public static bool SequenceEqual<T>(ref T first, ref T second, int length) 
        where T : IEquatable<T>
    {
        Debug.Assert(length >= 0);

        if (Unsafe.AreSame(ref first, ref second))
            goto Equal;

        nint index = 0; // Use nint for arithmetic to avoid unnecessary 64->32->64 truncations
        T lookUp0;
        T lookUp1;
        while (length >= 8)
        {
            length -= 8;

            lookUp0 = Unsafe.Add(ref first, index);
            lookUp1 = Unsafe.Add(ref second, index);
            if (!(lookUp0?.Equals(lookUp1) ?? (object?)lookUp1 is null))
                goto NotEqual;
            lookUp0 = Unsafe.Add(ref first, index + 1);
            lookUp1 = Unsafe.Add(ref second, index + 1);
            if (!(lookUp0?.Equals(lookUp1) ?? (object?)lookUp1 is null))
                goto NotEqual;
            lookUp0 = Unsafe.Add(ref first, index + 2);
            lookUp1 = Unsafe.Add(ref second, index + 2);
            if (!(lookUp0?.Equals(lookUp1) ?? (object?)lookUp1 is null))
                goto NotEqual;
            lookUp0 = Unsafe.Add(ref first, index + 3);
            lookUp1 = Unsafe.Add(ref second, index + 3);
            if (!(lookUp0?.Equals(lookUp1) ?? (object?)lookUp1 is null))
                goto NotEqual;
            lookUp0 = Unsafe.Add(ref first, index + 4);
            lookUp1 = Unsafe.Add(ref second, index + 4);
            if (!(lookUp0?.Equals(lookUp1) ?? (object?)lookUp1 is null))
                goto NotEqual;
            lookUp0 = Unsafe.Add(ref first, index + 5);
            lookUp1 = Unsafe.Add(ref second, index + 5);
            if (!(lookUp0?.Equals(lookUp1) ?? (object?)lookUp1 is null))
                goto NotEqual;
            lookUp0 = Unsafe.Add(ref first, index + 6);
            lookUp1 = Unsafe.Add(ref second, index + 6);
            if (!(lookUp0?.Equals(lookUp1) ?? (object?)lookUp1 is null))
                goto NotEqual;
            lookUp0 = Unsafe.Add(ref first, index + 7);
            lookUp1 = Unsafe.Add(ref second, index + 7);
            if (!(lookUp0?.Equals(lookUp1) ?? (object?)lookUp1 is null))
                goto NotEqual;

            index += 8;
        }

        if (length >= 4)
        {
            length -= 4;

            lookUp0 = Unsafe.Add(ref first, index);
            lookUp1 = Unsafe.Add(ref second, index);
            if (!(lookUp0?.Equals(lookUp1) ?? (object?)lookUp1 is null))
                goto NotEqual;
            lookUp0 = Unsafe.Add(ref first, index + 1);
            lookUp1 = Unsafe.Add(ref second, index + 1);
            if (!(lookUp0?.Equals(lookUp1) ?? (object?)lookUp1 is null))
                goto NotEqual;
            lookUp0 = Unsafe.Add(ref first, index + 2);
            lookUp1 = Unsafe.Add(ref second, index + 2);
            if (!(lookUp0?.Equals(lookUp1) ?? (object?)lookUp1 is null))
                goto NotEqual;
            lookUp0 = Unsafe.Add(ref first, index + 3);
            lookUp1 = Unsafe.Add(ref second, index + 3);
            if (!(lookUp0?.Equals(lookUp1) ?? (object?)lookUp1 is null))
                goto NotEqual;

            index += 4;
        }

        while (length > 0)
        {
            lookUp0 = Unsafe.Add(ref first, index);
            lookUp1 = Unsafe.Add(ref second, index);
            if (!(lookUp0?.Equals(lookUp1) ?? (object?)lookUp1 is null))
                goto NotEqual;
            index += 1;
            length--;
        }

    Equal:
        return true;

    NotEqual: // Workaround for https://github.com/dotnet/runtime/issues/8795
        return false;
    }
    
}