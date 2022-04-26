using System;

namespace Pentagon
{
    public class Kernel
    {

        public static int Main()
        {
            int a;
            try
            {
                throw new Exception();
            }
            catch (Exception e)
            {
                // watcher
                a = 1;
            }
            finally
            {
                a = 2;
            }

            return a;
        }
        
    }
}