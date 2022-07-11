using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;
using System.Buffers;

namespace Pentagon
{
    /// <summary>
    /// PCI-based device drivers singleton.
    /// There is *one* instance for each driver, and it can generate driven devices
    /// </summary>
    public interface IPciDriver
    {
        /// <summary>
        /// Try initializing the device with the current driver
        /// </summary>
        /// <returns>Returns false if device couldn't be initialized</returns>
        // TODO: differentiate between device can't be commanded and intialization failure
        public bool Init(PciDevice a);
    }
}
