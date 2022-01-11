namespace System
{
    /// <summary>
    /// Provides methods for creating, manipulating, searching, and sorting arrays, thereby serving as the base class for all arrays in
    /// the common language runtime.
    /// </summary>
    public abstract class Array
    {
        
        /// <summary>
        /// Gets the maximum number of elements that may be contained in an array.
        /// </summary>
        public static int MaxLength => Int32.MaxValue;

#pragma warning disable 649
        private int _length;
#pragma warning restore 649

        /// <summary>
        /// Gets the total number of elements in all the dimensions of the Array.
        /// </summary>
        public int Length => _length;

        /// <summary>
        /// Gets a 64-bit integer that represents the total number of elements in all the dimensions of the Array.
        /// </summary>
        public long LongLength => _length;
        
        /// <summary>
        /// Gets the rank (number of dimensions) of the Array. For example, a one-dimensional array returns 1, a two-dimensional array
        /// returns 2, and so on.
        ///
        /// TODO: Right now we only support rank 1 arrays 
        /// </summary>
        public int Rank => 1;

    }
}