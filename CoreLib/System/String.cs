namespace System
{
    /// <summary>
    /// Represents text as a sequence of UTF-16 code units.
    /// </summary>
    public sealed class String
    {

        /**
         * Under the hood this class is variable length, where the first int
         * is the length and the rest are UTF-16 chars, in order to have as less
         * c code as possible we are going to simply have the first char defined
         * and access it as a pointer. This does use unsafe code but it is fine
         * since it is part of the runtime
         *
         * TODO: define the class layout to make sure we don't do anything funny to
         */
        
#pragma warning disable 649
        private int _length;
#pragma warning restore 649
        private char _first;

        /// <summary>
        /// Gets the number of characters in the current String object.
        /// </summary>
        public int Length => _length;

        /// <summary>
        /// Gets the Char object at a specified position in the current String object.
        /// </summary>
        /// <param name="index">A position in the current string.</param>
        public char this[int index]
        {
            get
            {
                if (index >= _length)
                {
                    throw new IndexOutOfRangeException();
                }

                unsafe
                {
                    fixed (char* ptr = &_first)
                    {
                        return ptr[index];
                    }
                }
            }
        }
        
    }
}