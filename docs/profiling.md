# How to use the profiler
## Install Spall
Download and install Odin. The Windows version doesn't include wasm-ld, as far as I could tell, so it won't work.
NOTE: it doesn't work on Alpine either.
Clone `https://github.com/colrdavidson/spall`, and build+run with `python build.py run`.
Keeping the window open will minimize loading time on larger files.

## Run
Modify the Makefile to have those options.
TODO: GCC is needed for excluding directories from instrumentation, find a way around it
```
USE_GCC		:= 1
USE_PROF	:= 1
DEBUG		:= 1
```

Add `profiler_start()` and `profiler_stop()` around what you want to profile.
For example, `kernel.c` has
```c
profiler_start();
CHECK_AND_RETHROW(loader_load_corelib(m_corelib_file.address, m_corelib_file.size));
profiler_stop();
```
After `profiler_stop()`, it will write something like 
```
[*] Profiler finished: memsave 0xFFFFFFFF80180440 56907728 profiler.trace
```
Open the QEMU monitor and write the command to dump.

## Postprocessing
Build `scripts/profiler_gen` and run it, passing as a parameter the output path.
