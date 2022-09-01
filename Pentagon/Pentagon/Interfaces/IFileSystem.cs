using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;

namespace Pentagon.Interfaces;

/// <summary>
/// Represents a generic File System node, can be either file or directory
/// </summary>
public interface INode
{
    
    /// <summary>
    /// The size of the file in bytes.
    /// </summary>
    public long FileSize { get; }

    /// <summary>
    /// The amount of physical space the file consumes on the file system volume.
    /// </summary>
    public long PhysicalSize { get; }
    
    // public DateTime CreateTime { get; }
    //
    // public DateTime LastAccessTime { get; }
    //
    // public DateTime ModificationTime { get; }

    /// <summary>
    /// The name of the file.
    /// </summary>
    public string FileName { get; }

    /// <summary>
    /// Delete the file.
    /// </summary>
    /// <param name="token"></param>
    /// <returns></returns>
    public Task Delete(CancellationToken token = default);

    /// <summary>
    /// Flushes all modified data associated with a file to a device.
    /// </summary>
    /// <param name="token"></param>
    /// <returns></returns>
    public Task Flush(CancellationToken token = default);
}

[Flags]
public enum FileOpenMode
{
    Read = (1 << 0),
    Write = (1 << 1),
    Create = (1 << 2)
}

/// <summary>
/// Represents a directory in the file system
/// </summary>
public interface IDirectory : INode
{
    
    /// <summary>
    /// Opens a new file relative to the source file's location
    /// </summary>
    /// <param name="filename">
    /// name of the file to be opened.
    /// The file name may contain the following path modifiers: "\", ".", ".."
    /// </param>
    /// <param name="mode">
    /// The mode to open the file.
    /// The only valid combinations that the file may be opened with are: Read, Read/Write or Create/Read/Write
    /// </param>
    /// <param name="token"></param>
    /// <returns></returns>
    public Task<IFile> OpenFile(string filename, FileOpenMode mode, CancellationToken token = default);

    /// <summary>
    /// Opens a new directory relative to the source file's location
    /// </summary>
    /// <param name="filename">
    /// name of the file to be opened.
    /// The file name may contain the following path modifiers: "\", ".", ".."
    /// </param>
    /// <param name="mode">
    /// The mode to open the file.
    /// The only valid combinations that the file may be opened with are: Read, Read/Write or Create/Read/Write
    /// </param>
    /// <param name="token"></param>
    /// <returns></returns>
    public Task<IDirectory> OpenDirectory(string filename, FileOpenMode mode, CancellationToken token = default);

    /// <summary>
    /// Get all the files 
    /// </summary>
    /// <param name="token"></param>
    /// <returns></returns>
    public IAsyncEnumerable<INode> GetAsyncEnumerator(CancellationToken token = default);

}

/// <summary>
/// Represents a file in the file system.
/// </summary>
public interface IFile : INode
{
    
    /// <summary>
    /// Reads data from a file
    /// </summary>
    /// <param name="offset">The offset in the file</param>
    /// <param name="buffer">Span to the output of the data, the read size is the Span size</param>
    /// <param name="token"></param>
    /// <returns>The amount actually read from the file</returns>
    public Task<int> Read(long offset, Memory<byte> buffer, CancellationToken token = default);

    // TODO: Use ReadOnlySpan instead when we implement one 
    /// <summary>
    /// Writes data to a file 
    /// </summary>
    /// <param name="offset">The offset in the file</param>
    /// <param name="buffer">Span to the output of the data, write size is the Span size</param>
    /// <param name="token"></param>
    /// <returns></returns>
    public Task<int> Write(long offset, Memory<byte> buffer, CancellationToken token = default);

}

/// <summary>
/// Represents a directory in the file system
/// </summary>
public interface IFileSystem
{
    
    /// <summary>
    /// true if the volume only supports read access.
    /// </summary>
    public bool ReadOnly { get; }
    
    /// <summary>
    /// The number of bytes managed by the file system.
    /// </summary>
    public long VolumeSize { get; }
    
    /// <summary>
    /// The number of available bytes for use by the file system.
    /// </summary>
    public long FreeSpace { get; }
    
    /// <summary>
    /// The nominal block size by which files are typically grown.
    /// </summary>
    public long BlockSize { get; }
    
    /// <summary>
    /// The volume's label.
    /// </summary>
    public string VolumeLabel { get; }

    /// <summary>
    /// Open the root directory on a volume
    /// </summary>
    /// <returns>Opened file handle for the root directory</returns>
    public Task<IDirectory> OpenVolume();

}