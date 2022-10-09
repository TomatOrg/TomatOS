// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

namespace System.Threading
{
    public class LockRecursionException : System.Exception
    {
        public LockRecursionException()
        {
        }

        public LockRecursionException(string? message)
            : base(message)
        {
        }

        public LockRecursionException(string? message, Exception? innerException)
            : base(message, innerException)
        {
        }
    }
}
