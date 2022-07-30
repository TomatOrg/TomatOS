namespace System;

public class ObjectDisposedException : InvalidOperationException
{

    private readonly string _objectName;

    /// <summary>
    /// Gets the text for the message for this exception.
    /// </summary>
    public override string Message
    {
        get
        {
            var name = ObjectName;
            if (string.IsNullOrEmpty(name))
            {
                return base.Message;
            }

            var objectDisposed = $"Object name: '{name}'.";
            return $"{base.Message}\n{objectDisposed}";
        }
    }

    public string ObjectName => _objectName ?? string.Empty;

    public ObjectDisposedException()
        : base("Cannot access a disposed object.")
    {
    }

    public ObjectDisposedException(string objectName)
        : this(objectName, "Cannot access a disposed object.")
    {
    }

    public ObjectDisposedException(string objectName, string message) 
        : base(message)
    {
        _objectName = objectName;
    }
    
    public ObjectDisposedException(string message, Exception innerException)
        : base(message, innerException)
    {
    }

}