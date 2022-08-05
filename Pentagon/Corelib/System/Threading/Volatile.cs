using System.Runtime.CompilerServices;
using System.Runtime.Versioning;

namespace System.Threading
{
    /// <summary>Methods for accessing memory with volatile semantics.</summary>
    public static unsafe class Volatile
    {
        public static short Read(ref short location) => location;

        public static void Write(ref short location, short value) => location = value;

        public static int Read(ref int location) => location;

        public static void Write(ref int location, int value) => location = value;

        public static T Read<T>(ref T location) where T : class? => location;

        public static void Write<T>(ref T location, T value) where T : class? => location = value;
    }
}
