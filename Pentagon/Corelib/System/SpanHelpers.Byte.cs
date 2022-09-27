// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System.Diagnostics;
using System.Numerics;
using System.Runtime.CompilerServices;

namespace System;

internal static partial class SpanHelpers
{
    
    // Optimized byte-based SequenceEquals. The "length" parameter for this one is declared a nuint rather than int as we also use it for types other than byte
    // where the length can exceed 2Gb once scaled by sizeof(T).
    public static unsafe bool SequenceEqual(ref byte first, ref byte second, nuint length)
    {
        bool result;
        // Use nint for arithmetic to avoid unnecessary 64->32->64 truncations
        if (length >= (nuint)sizeof(nuint))
        {
            // Conditional jmp foward to favor shorter lengths. (See comment at "Equal:" label)
            // The longer lengths can make back the time due to branch misprediction
            // better than shorter lengths.
            goto Longer;
        }

        if (length < sizeof(uint))
        {
            uint differentBits = 0;
            nuint offset = (length & 2);
            if (offset != 0)
            {
                differentBits = LoadUShort(ref first);
                differentBits -= LoadUShort(ref second);
            }
            if ((length & 1) != 0)
            {
                differentBits |= (uint)Unsafe.AddByteOffset(ref first, offset) - (uint)Unsafe.AddByteOffset(ref second, offset);
            }
            result = (differentBits == 0);
            goto Result;
        }
        else
        {
            nuint offset = length - sizeof(uint);
            uint differentBits = LoadUInt(ref first) - LoadUInt(ref second);
            differentBits |= LoadUInt(ref first, offset) - LoadUInt(ref second, offset);
            result = (differentBits == 0);
            goto Result;
        }

    Longer:
        // Only check that the ref is the same if buffers are large,
        // and hence its worth avoiding doing unnecessary comparisons
        if (!Unsafe.AreSame(ref first, ref second))
        {
            // C# compiler inverts this test, making the outer goto the conditional jmp.
            goto Vector;
        }

        // This becomes a conditional jmp foward to not favor it.
        goto Equal;

    Result:
        return result;
    // When the sequence is equal; which is the longest execution, we want it to determine that
    // as fast as possible so we do not want the early outs to be "predicted not taken" branches.
    Equal:
        return true;

    Vector:
        Debug.Assert(length >= (nuint)sizeof(nuint));
        
        // TODO: SIMD extensions
        
        {
            nuint offset = 0;
            nuint lengthToExamine = length - (nuint)sizeof(nuint);
            // Unsigned, so it shouldn't have overflowed larger than length (rather than negative)
            Debug.Assert(lengthToExamine < length);
            if (lengthToExamine > 0)
            {
                do
                {
                    // Compare unsigned so not do a sign extend mov on 64 bit
                    if (LoadNUInt(ref first, offset) != LoadNUInt(ref second, offset))
                    {
                        goto NotEqual;
                    }
                    offset += (nuint)sizeof(nuint);
                } while (lengthToExamine > offset);
            }

            // Do final compare as sizeof(nuint) from end rather than start
            result = (LoadNUInt(ref first, lengthToExamine) == LoadNUInt(ref second, lengthToExamine));
            goto Result;
        }

        // As there are so many true/false exit points the Jit will coalesce them to one location.
        // We want them at the end so the conditional early exit jmps are all jmp forwards so the
        // branch predictor in a uninitialized state will not take them e.g.
        // - loops are conditional jmps backwards and predicted
        // - exceptions are conditional fowards jmps and not predicted
    NotEqual:
        return false;
    }
    
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private static ushort LoadUShort(ref byte start)
        => Unsafe.ReadUnaligned<ushort>(ref start);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private static uint LoadUInt(ref byte start)
        => Unsafe.ReadUnaligned<uint>(ref start);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private static uint LoadUInt(ref byte start, nuint offset)
        => Unsafe.ReadUnaligned<uint>(ref Unsafe.AddByteOffset(ref start, offset));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private static nuint LoadNUInt(ref byte start)
        => Unsafe.ReadUnaligned<nuint>(ref start);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private static nuint LoadNUInt(ref byte start, nuint offset)
        => Unsafe.ReadUnaligned<nuint>(ref Unsafe.AddByteOffset(ref start, offset));

}