# Pentagon

An experiment for writing a kernel with C# drivers and usermode, with a fully featured dotnet runtime running on 
baremetal with the main goal of using Type safety for memory safety.

Unlike other baremetal C# runtimes out there (For example [Cosmos](https://github.com/CosmosOS/Cosmos)) we have a full 
garbage collector and JIT support in the kernel itself, so stuff like reflection/emitters and more will eventually be 
available for apps to use.

## Progress

### Kernel itself
- Memory subsystem
  - Buddy-bitmap-tree based physical memory allocator
  - TLSF based kernel heap
  - Single address space management
- Threading subsystem
  - Lightweight threading
  - Mostly copied (translated) from Go
    - Scheduler
    - Synchronization primitives
    - Timer subsystem

### Dotnet Runtime
Right now the main work is on the runtime itself, the main features of the runtime:
- On-the-fly Garbage Collector for pause free garbage collection
  - Including support for finalization and reviving
  - A really cheap write-barrier
- Full support for reference types
  - With abstract/virtual methods support
  - Upcasting fully implemented
- Full support for integer and floating point types
- Full support for array types
- Full support for struct types
- Full support for interface types
  - Implemented using Fat-Pointers and implicitly casting as needed
- Full support for managed references
  - Supports locals, fields and array elements
- Full support for boxing/unboxing 
- Most common CIL instructions implemented
- Visibility and Accessibility checking

### Main missing features
- Bit-shifting
- Generics
- Delegates
- Overflow math

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
