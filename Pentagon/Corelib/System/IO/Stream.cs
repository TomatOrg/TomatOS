namespace System.IO;

public abstract class Stream : IDisposable
{

    public abstract bool CanRead { get; }
    public abstract bool CanSeek { get; }
    public abstract bool CanWrite { get; }
    public virtual bool CanTimeout => false;
    
    public abstract long Length { get; }
    public abstract long Position { get; set; }
    
    public virtual int ReadTimeout
    {
        get => throw new InvalidOperationException("Timeouts are not supported on this stream.");
        set => throw new InvalidOperationException("Timeouts are not supported on this stream.");
    }

    public virtual int WriteTimeout
    {
        get => throw new InvalidOperationException("Timeouts are not supported on this stream.");
        set => throw new InvalidOperationException("Timeouts are not supported on this stream.");
    }

    protected Stream()
    {
    }

    public virtual void Close()
    {
        Dispose(true);
        GC.SuppressFinalize(this);
    }

    public void Dispose()
    {
        Close();
    }
    
    protected virtual void Dispose(bool disposing)
    {
    }

    #region Null Stream

    public static readonly Stream Null = new NullStream();

    private sealed class NullStream : Stream
    {
        public override bool CanRead => true;
        public override bool CanSeek => true;
        public override bool CanWrite => true;
        public override long Length => 0;

        public override long Position
        {
            get => 0;
            set { }
        }
        
    }
    
    #endregion

    #region Synchronized region

    public static Stream Synchronized(Stream stream)
    {
        return new SyncStream(stream);
    }

    private sealed class SyncStream : Stream
    {
        private Stream _stream;

        public override bool CanRead => _stream.CanRead;
        public override bool CanSeek => _stream.CanSeek;
        public override bool CanWrite => _stream.CanWrite;
        public override bool CanTimeout => _stream.CanTimeout;

        public override int ReadTimeout
        {
            get => _stream.ReadTimeout;
            set => _stream.ReadTimeout = value;
        }

        public override int WriteTimeout
        {
            get => _stream.WriteTimeout;
            set => _stream.WriteTimeout = value;
        }

        public override long Length
        {
            get
            {
                lock (_stream)
                {
                    return _stream.Length;
                }
            }
        }

        public override long Position
        {
            get 
            {
                lock (_stream)
                {
                    return _stream.Position;
                }
            }
            set
            {
                lock (_stream)
                {
                    _stream.Position = value;
                }
            }
        }

        internal SyncStream(Stream stream)
        {
            _stream = stream;
        }

        public override void Close()
        {
            lock (_stream)
            {
                try
                {
                    _stream.Close();
                }
                finally
                {
                    base.Dispose(true);
                }
            }
        }

        protected override void Dispose(bool disposing)
        {
            lock (_stream)
            {
                try
                {
                    if (disposing)
                    {
                        _stream.Dispose();
                    }
                }
                finally
                {
                    base.Dispose(disposing);
                }
            }
        }
    }

    #endregion
    
}