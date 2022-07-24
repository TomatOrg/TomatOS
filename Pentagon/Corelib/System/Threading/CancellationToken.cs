// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System.Diagnostics;

namespace System.Threading
{
    /// <summary>
    /// Propagates notification that operations should be canceled.
    /// </summary>
    /// <remarks>
    /// <para>
    /// A <see cref="CancellationToken"/> may be created directly in an unchangeable canceled or non-canceled state
    /// using the CancellationToken's constructors. However, to have a CancellationToken that can change
    /// from a non-canceled to a canceled state,
    /// <see cref="System.Threading.CancellationTokenSource">CancellationTokenSource</see> must be used.
    /// CancellationTokenSource exposes the associated CancellationToken that may be canceled by the source through its
    /// <see cref="System.Threading.CancellationTokenSource.Token">Token</see> property.
    /// </para>
    /// <para>
    /// Once canceled, a token may not transition to a non-canceled state, and a token whose
    /// <see cref="CanBeCanceled"/> is false will never change to one that can be canceled.
    /// </para>
    /// <para>
    /// All members of this struct are thread-safe and may be used concurrently from multiple threads.
    /// </para>
    /// </remarks>
    public readonly struct CancellationTokenRegistration : IEquatable<CancellationTokenRegistration>, IDisposable /*, IAsyncDisposable */{

        public void Dispose()
        {
        }
        public bool Equals(CancellationTokenRegistration other) => true;
    }
    public readonly struct CancellationToken : IEquatable<CancellationToken>
    {
        public static CancellationToken None => default;
        public bool Equals(CancellationToken other) => true;

        public void ThrowIfCancellationRequested()
        {
        }
        public bool IsCancellationRequested => false;
        public bool CanBeCanceled => false;

        public CancellationTokenRegistration UnsafeRegister(Action<object?> callback, object? state) => new();
    }
}