# TomatOS

An experiment for writing a kernel with C# drivers and usermode, with a fully featured dotnet runtime running on 
baremetal with the main goal of using Type safety for memory safety.

Unlike other baremetal C# runtimes out there (For example [Cosmos](https://github.com/CosmosOS/Cosmos)) we have a full 
garbage collector and JIT support in the kernel itself, so stuff like reflection/emitters and more will eventually be 
available for apps to use.

## Progress

### Kernel itself
- Memory subsystem
  - Freelist based physical memory allocator
  - TLSF based kernel heap
  - Single address space management
- Threading subsystem
  - Lightweight threading
  - Mostly copied (translated) from Go
    - Scheduler
    - Synchronization primitives
    - Timer subsystem

### Dotnet Runtime
We are using our custom dotnet runtime called [TinyDotNet](https://github.com/TomatOrg/tinydotnet), for progress on it see its README

## OS Design

- Everything runs in a single cpu address space
  - Threads are super light-weight and different assemblies have no context switch overhead. 
- Safety by Type system
  - Accessibility/Visibility is enforced by the kernel at JIT time
      - If you don't have access to a field you have no way to get its value
  - If you don't have a ref to an object and you can't create it yourself, you won't have 
    any way to get a reference to it 
- Each assembly has one global instance, for GUI apps the window serveris going to simply notify you that you should 
  open a new window/application instance.

# Licenses
- The Native Kernel is published under GPLv2
- The Managed Kernel is published under LGPLv3
- The Corelib is published under MIT
- The .NET Runtime is published under MIT  
