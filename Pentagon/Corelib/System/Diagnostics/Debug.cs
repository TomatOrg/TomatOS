using System;
using System.Collections.Generic;
using System.Threading.Tasks;

namespace System.Diagnostics
{
    internal class Debug
    {
        public static void Assert(bool b, string s)
        {
            if (!b)
            {
            }
        }
        public static void Assert(bool b)
        {
            if (!b)
            {
            }
        }
        public static void Fail(string s)
        {
        }
    }

}
