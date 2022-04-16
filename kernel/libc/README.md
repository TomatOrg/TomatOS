# Dummy libc

Some libraries we use rely on libc, so for being able to run the libraries 
without modifying their source code we have this thin wrapper that will 
hopefully give us everything we need to compile and run these libraries 
properly.
