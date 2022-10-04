// NextUInt64 is based on the algorithm from http://prng.di.unimi.it/xoshiro256starstar.c:
//
//     Written in 2018 by David Blackman and Sebastiano Vigna (vigna@acm.org)
//
//     To the extent possible under law, the author has dedicated all copyright
//     and related and neighboring rights to this software to the public domain
//     worldwide. This software is distributed without any warranty.
//
//     See <http://creativecommons.org/publicdomain/zero/1.0/>.

using System.Diagnostics;
using System.Numerics;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Threading;

namespace System;

public class Random
{
    
    public static Random Shared = new ThreadSafeRandom();

    public Random()
        : this((int)Stopwatch.GetTimestamp())
    {
    }

    private ulong _s0;
    private ulong _s1;
    private ulong _s2;
    private ulong _s3;

    public Random(int seed)
    {
        var lseed = (ulong)seed;
        ulong SplitMix64()
        {
            var z = (lseed += 0x9e3779b97f4a7c15);
            z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
            z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
            return z ^ (z >> 31);
        }
        
        _s0 = SplitMix64();
        _s1 = SplitMix64();
        _s2 = SplitMix64();
        _s3 = SplitMix64();
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal ulong NextUInt64()
    {
        var s0 = _s0;
        var s1 = _s1;
        var s2 = _s2;
        var s3 = _s3;

        var result = BitOperations.RotateLeft(s1 * 5, 7) * 9;
        var t = s1 << 17;

        s2 ^= s0;
        s3 ^= s1;
        s1 ^= s2;
        s0 ^= s3;

        s2 ^= t;
        s3 = BitOperations.RotateLeft(s3, 45);

        _s0 = s0;
        _s1 = s1;
        _s2 = s2;
        _s3 = s3;

        return result;
    }

    /// <summary>Returns a non-negative random integer.</summary>
    /// <returns>A 32-bit signed integer that is greater than or equal to 0 and less than <see cref="int.MaxValue"/>.</returns>
    public virtual int Next()
    {
        while (true)
        {
            // Get top 31 bits to get a value in the range [0, int.MaxValue], but try again
            // if the value is actually int.MaxValue, as the method is defined to return a value
            // in the range [0, int.MaxValue).
            ulong result = NextUInt64() >> 33;
            if (result != int.MaxValue)
            {
                return (int)result;
            }
        }
    }

    /// <summary>Returns a non-negative random integer that is less than the specified maximum.</summary>
    /// <param name="maxValue">The exclusive upper bound of the random number to be generated. <paramref name="maxValue"/> must be greater than or equal to 0.</param>
    /// <returns>
    /// A 32-bit signed integer that is greater than or equal to 0, and less than <paramref name="maxValue"/>; that is, the range of return values ordinarily
    /// includes 0 but not <paramref name="maxValue"/>. However, if <paramref name="maxValue"/> equals 0, <paramref name="maxValue"/> is returned.
    /// </returns>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="maxValue"/> is less than 0.</exception>
    public virtual int Next(int maxValue)
    {
        if (maxValue < 0)
        {
            ThrowMaxValueMustBeNonNegative();
        }

        if (maxValue > 1)
        {
            // Narrow down to the smallest range [0, 2^bits] that contains maxValue.
            // Then repeatedly generate a value in that outer range until we get one within the inner range.
            int bits = BitOperations.Log2Ceiling((uint)maxValue);
            while (true)
            {
                ulong result = NextUInt64() >> (sizeof(ulong) * 8 - bits);
                if (result < (uint)maxValue)
                {
                    return (int)result;
                }
            }
        }

        return 0;
    }

    /// <summary>Returns a random integer that is within a specified range.</summary>
    /// <param name="minValue">The inclusive lower bound of the random number returned.</param>
    /// <param name="maxValue">The exclusive upper bound of the random number returned. <paramref name="maxValue"/> must be greater than or equal to <paramref name="minValue"/>.</param>
    /// <returns>
    /// A 32-bit signed integer greater than or equal to <paramref name="minValue"/> and less than <paramref name="maxValue"/>; that is, the range of return values includes <paramref name="minValue"/>
    /// but not <paramref name="maxValue"/>. If minValue equals <paramref name="maxValue"/>, <paramref name="minValue"/> is returned.
    /// </returns>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="minValue"/> is greater than <paramref name="maxValue"/>.</exception>
    public virtual int Next(int minValue, int maxValue)
    {
        if (minValue > maxValue)
        {
            ThrowMinMaxValueSwapped();
        }
        
        var exclusiveRange = (ulong)((long)maxValue - minValue);

        if (exclusiveRange > 1)
        {
            // Narrow down to the smallest range [0, 2^bits] that contains maxValue.
            // Then repeatedly generate a value in that outer range until we get one within the inner range.
            int bits = BitOperations.Log2Ceiling(exclusiveRange);
            while (true)
            {
                ulong result = NextUInt64() >> (sizeof(ulong) * 8 - bits);
                if (result < exclusiveRange)
                {
                    return (int)result + minValue;
                }
            }
        }
        
        return minValue;
    }

    /// <summary>Returns a non-negative random integer.</summary>
    /// <returns>A 64-bit signed integer that is greater than or equal to 0 and less than <see cref="long.MaxValue"/>.</returns>
    public virtual long NextInt64()
    {
        while (true)
        {
            // Get top 63 bits to get a value in the range [0, long.MaxValue], but try again
            // if the value is actually long.MaxValue, as the method is defined to return a value
            // in the range [0, long.MaxValue).
            ulong result = NextUInt64() >> 1;
            if (result != long.MaxValue)
            {
                return (long)result;
            }
        }
    }

    /// <summary>Returns a non-negative random integer that is less than the specified maximum.</summary>
    /// <param name="maxValue">The exclusive upper bound of the random number to be generated. <paramref name="maxValue"/> must be greater than or equal to 0.</param>
    /// <returns>
    /// A 64-bit signed integer that is greater than or equal to 0, and less than <paramref name="maxValue"/>; that is, the range of return values ordinarily
    /// includes 0 but not <paramref name="maxValue"/>. However, if <paramref name="maxValue"/> equals 0, <paramref name="maxValue"/> is returned.
    /// </returns>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="maxValue"/> is less than 0.</exception>
    public virtual long NextInt64(long maxValue)
    {
        if (maxValue < 0)
        {
            ThrowMaxValueMustBeNonNegative();
        }

        if (maxValue > 1)
        {
            // Narrow down to the smallest range [0, 2^bits] that contains maxValue.
            // Then repeatedly generate a value in that outer range until we get one within the inner range.
            int bits = BitOperations.Log2Ceiling((ulong)maxValue);
            while (true)
            {
                ulong result = NextUInt64() >> (sizeof(ulong) * 8 - bits);
                if (result < (ulong)maxValue)
                {
                    return (long)result;
                }
            }
        }
        
        return 0;
    }

    /// <summary>Returns a random integer that is within a specified range.</summary>
    /// <param name="minValue">The inclusive lower bound of the random number returned.</param>
    /// <param name="maxValue">The exclusive upper bound of the random number returned. <paramref name="maxValue"/> must be greater than or equal to <paramref name="minValue"/>.</param>
    /// <returns>
    /// A 64-bit signed integer greater than or equal to <paramref name="minValue"/> and less than <paramref name="maxValue"/>; that is, the range of return values includes <paramref name="minValue"/>
    /// but not <paramref name="maxValue"/>. If minValue equals <paramref name="maxValue"/>, <paramref name="minValue"/> is returned.
    /// </returns>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="minValue"/> is greater than <paramref name="maxValue"/>.</exception>
    public virtual long NextInt64(long minValue, long maxValue)
    {
        if (minValue > maxValue)
        {
            ThrowMinMaxValueSwapped();
        }
        
        ulong exclusiveRange = (ulong)(maxValue - minValue);

        if (exclusiveRange > 1)
        {
            // Narrow down to the smallest range [0, 2^bits] that contains maxValue.
            // Then repeatedly generate a value in that outer range until we get one within the inner range.
            int bits = BitOperations.Log2Ceiling(exclusiveRange);
            while (true)
            {
                ulong result = NextUInt64() >> (sizeof(ulong) * 8 - bits);
                if (result < exclusiveRange)
                {
                    return (long)result + minValue;
                }
            }
        }

        return minValue;
    }

    /// <summary>Fills the elements of a specified array of bytes with random numbers.</summary>
    /// <param name="buffer">The array to be filled with random numbers.</param>
    /// <exception cref="ArgumentNullException"><paramref name="buffer"/> is null.</exception>
    public virtual void NextBytes(byte[] buffer)
    {
        if (buffer is null)
        {
            ThrowHelper.ThrowArgumentNullException(ExceptionArgument.buffer);
        }

        NextBytes(buffer.AsSpan());
    }

    /// <summary>Fills the elements of a specified span of bytes with random numbers.</summary>
    /// <param name="buffer">The array to be filled with random numbers.</param>
    public virtual void NextBytes(Span<byte> buffer)
    {
        while (buffer.Length >= sizeof(ulong))
        {
            Unsafe.WriteUnaligned(ref buffer[0], NextUInt64());
            buffer = buffer.Slice(sizeof(ulong));
        }

        if (!buffer.IsEmpty)
        {
            var value = NextUInt64();
            for (var i = 0; i < buffer.Length; i++)
            {
                var b = (byte)(value & 0xFF);
                value >>= 8;
                buffer[i] = b;
            }
        }
    }
    
    /// <summary>Returns a random floating-point number that is greater than or equal to 0.0, and less than 1.0.</summary>
    /// <returns>A double-precision floating point number that is greater than or equal to 0.0, and less than 1.0.</returns>
    public virtual double NextDouble()
    {
        // As described in http://prng.di.unimi.it/:
        // "A standard double (64-bit) floating-point number in IEEE floating point format has 52 bits of significand,
        //  plus an implicit bit at the left of the significand. Thus, the representation can actually store numbers with
        //  53 significant binary digits. Because of this fact, in C99 a 64-bit unsigned integer x should be converted to
        //  a 64-bit double using the expression
        //  (x >> 11) * 0x1.0p-53"
        return (NextUInt64() >> 11) * (1.0 / (1ul << 53));
    }

    /// <summary>Returns a random floating-point number that is greater than or equal to 0.0, and less than 1.0.</summary>
    /// <returns>A single-precision floating point number that is greater than or equal to 0.0, and less than 1.0.</returns>
    public virtual float NextSingle()
    {
        // Same as above, but with 24 bits instead of 53.
        return (NextUInt64() >> 40) * (1.0f / (1u << 24));
    }

    private static void ThrowMaxValueMustBeNonNegative() =>
        throw new ArgumentOutOfRangeException("maxValue", ArgumentOutOfRangeException.NeedNonNegNum);

    private static void ThrowMinMaxValueSwapped() =>
        throw new ArgumentOutOfRangeException("minValue", $"'minValue' cannot be greater than maxValue");
    
    private class ThreadSafeRandom : Random
    {
        
        private ulong _x = 0;

        public ThreadSafeRandom(ulong seed)
        {
            _x = seed;
        }

        public ThreadSafeRandom()
        {
            _x = (ulong)Stopwatch.GetTimestamp();
        }
    
        /// <summary>
        /// we are using splitmax64 (https://prng.di.unimi.it/splitmix64.c) because it is very fast
        /// and it is useful for us because it has only 64bit state, which means we can easily make
        /// it atomic which is perfect for the shared random number generator. 
        /// </summary>
        [MethodImpl(MethodImplOptions.AggressiveInlining)] 
        private ulong NextUInt64()
        {
            var z = Interlocked.Add(ref _x, 0x9e3779b97f4a7c15);
            z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
            z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
            return z ^ (z >> 31);
        }

        public virtual int Next()
        {
            while (true)
            {
                // Get top 31 bits to get a value in the range [0, int.MaxValue], but try again
                // if the value is actually int.MaxValue, as the method is defined to return a value
                // in the range [0, int.MaxValue).
                ulong result = NextUInt64() >> 33;
                if (result != int.MaxValue)
                {
                    return (int)result;
                }
            }
        }

        public virtual int Next(int maxValue)
        {
            if (maxValue < 0)
            {
                ThrowMaxValueMustBeNonNegative();
            }

            if (maxValue > 1)
            {
                // Narrow down to the smallest range [0, 2^bits] that contains maxValue.
                // Then repeatedly generate a value in that outer range until we get one within the inner range.
                int bits = BitOperations.Log2Ceiling((uint)maxValue);
                while (true)
                {
                    ulong result = NextUInt64() >> (sizeof(ulong) * 8 - bits);
                    if (result < (uint)maxValue)
                    {
                        return (int)result;
                    }
                }
            }

            return 0;
        }

        public virtual int Next(int minValue, int maxValue)
        {
            if (minValue > maxValue)
            {
                ThrowMinMaxValueSwapped();
            }
            
            var exclusiveRange = (ulong)((long)maxValue - minValue);

            if (exclusiveRange > 1)
            {
                // Narrow down to the smallest range [0, 2^bits] that contains maxValue.
                // Then repeatedly generate a value in that outer range until we get one within the inner range.
                int bits = BitOperations.Log2Ceiling(exclusiveRange);
                while (true)
                {
                    ulong result = NextUInt64() >> (sizeof(ulong) * 8 - bits);
                    if (result < exclusiveRange)
                    {
                        return (int)result + minValue;
                    }
                }
            }

            return minValue;
        }

        public virtual long NextInt64()
        {
            while (true)
            {
                // Get top 63 bits to get a value in the range [0, long.MaxValue], but try again
                // if the value is actually long.MaxValue, as the method is defined to return a value
                // in the range [0, long.MaxValue).
                ulong result = NextUInt64() >> 1;
                if (result != long.MaxValue)
                {
                    return (long)result;
                }
            }
        }

        public virtual long NextInt64(long maxValue)
        {
            if (maxValue < 0)
            {
                ThrowMaxValueMustBeNonNegative();
            }

            if (maxValue > 1)
            {
                // Narrow down to the smallest range [0, 2^bits] that contains maxValue.
                // Then repeatedly generate a value in that outer range until we get one within the inner range.
                int bits = BitOperations.Log2Ceiling((ulong)maxValue);
                while (true)
                {
                    ulong result = NextUInt64() >> (sizeof(ulong) * 8 - bits);
                    if (result < (ulong)maxValue)
                    {
                        return (long)result;
                    }
                }
            }

            return 0;
        }

        public virtual long NextInt64(long minValue, long maxValue)
        {
            if (minValue > maxValue)
            {
                ThrowMinMaxValueSwapped();
            }

            var exclusiveRange = (ulong)(maxValue - minValue);

            if (exclusiveRange > 1)
            {
                // Narrow down to the smallest range [0, 2^bits] that contains maxValue.
                // Then repeatedly generate a value in that outer range until we get one within the inner range.
                int bits = BitOperations.Log2Ceiling(exclusiveRange);
                while (true)
                {
                    ulong result = NextUInt64() >> (sizeof(ulong) * 8 - bits);
                    if (result < exclusiveRange)
                    {
                        return (long)result + minValue;
                    }
                }
            }
            
            return minValue;
        }

        public virtual void NextBytes(Span<byte> buffer)
        {
            while (buffer.Length >= sizeof(ulong))
            {
                Unsafe.WriteUnaligned(ref buffer[0], NextUInt64());
                buffer = buffer.Slice(sizeof(ulong));
            }

            if (!buffer.IsEmpty)
            {
                var value = NextUInt64();
                for (var i = 0; i < buffer.Length; i++)
                {
                    var b = (byte)(value & 0xFF);
                    value >>= 8;
                    buffer[i] = b;
                }
            }
        }

        public virtual double NextDouble()
        {
            // As described in http://prng.di.unimi.it/:
            // "A standard double (64-bit) floating-point number in IEEE floating point format has 52 bits of significand,
            //  plus an implicit bit at the left of the significand. Thus, the representation can actually store numbers with
            //  53 significant binary digits. Because of this fact, in C99 a 64-bit unsigned integer x should be converted to
            //  a 64-bit double using the expression
            //  (x >> 11) * 0x1.0p-53"
            return (NextUInt64() >> 11) * (1.0 / (1ul << 53));
        }

        public virtual float NextSingle()
        {
            // Same as above, but with 24 bits instead of 53.
            return (NextUInt64() >> 40) * (1.0f / (1u << 24));
        }
    }
    
}