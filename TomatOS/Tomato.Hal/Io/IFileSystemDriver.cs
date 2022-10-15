using Tomato.Hal.Interfaces;

namespace Tomato.Hal.Io;

public interface IFileSystemDriver
{

    /// <summary>
    /// Try to create a file system on the given block device, return NULL if not a compatible
    /// filesystem, returns an instance of the filesystem if compatible
    /// </summary>
    IFileSystem TryCreate(IBlock block);

}