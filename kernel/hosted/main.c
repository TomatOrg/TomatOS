#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/io.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <stdint.h>
#include <stdarg.h>

void load_file(const char* name, void** file, uint64_t* size) {
    struct stat s;
    int fd = open(name, O_RDONLY);
    fstat(fd, &s);
    *size = s.st_size;
    *file = mmap(0, *size, PROT_READ, MAP_PRIVATE, fd, 0);
}

uint64_t microtime() {
    struct timeval t;
    gettimeofday(&t, NULL);
    return ((uint64_t)t.tv_sec * 1000000) + t.tv_usec;
}

void *corelib_file, *kernel_file;
size_t corelib_file_size, kernel_file_size;

#define TRACE(fmt, ...) printf("[*] " fmt "\r\n", ## __VA_ARGS__)

int printf_(char const *fmt, ...) {
    va_list arg;
    int length;

    va_start(arg, fmt);
    length = vprintf(fmt, arg);
    va_end(arg);
    return length;
}
int snprintf_(char *str, size_t n, char const *fmt, ...) {
    va_list arg;
    int length;

    va_start(arg, fmt);
    length = vsnprintf(str, n, fmt, arg);
    va_end(arg);
    return length;
}

#include <runtime/dotnet/loader.h>
#include <runtime/dotnet/jit/jit.h>

int main() {
    load_file("Pentagon/Corelib/bin/Release/net6.0/Corelib.dll", &corelib_file, &corelib_file_size);
    load_file("Pentagon/Pentagon/bin/Release/net6.0/Pentagon.dll", &kernel_file, &kernel_file_size);
    init_jit();
    
    // load the corelib
    uint64_t start = microtime();
    loader_load_corelib(corelib_file, corelib_file_size);
    printf("corelib loading took %lums\n", (microtime() - start) / 1000);

    start = microtime();
    System_Reflection_Assembly kernel_asm = NULL;
    loader_load_assembly(kernel_file, kernel_file_size, &kernel_asm);
    printf("kernel loading took %dms\n", (microtime() - start) / 1000);

    method_result_t(*entry_point)() = kernel_asm->EntryPoint->MirFunc->addr;
    method_result_t result = entry_point();
    //CHECK(result.exception == NULL, "Got exception: \"%U\"", result.exception->Message);
    printf("Kernel output: %d\n", result.value);
}


// ah yes, syncronization
void scheduler_preempt_disable() {}
void scheduler_preempt_enable() {}
void spinlock_lock() {}
void spinlock_unlock() {}
void mutex_lock() {}
void mutex_unlock() {}

System_Object heap_find_fast(void *ptr) {
    return NULL;
}

// garbage "collection"
void* gc_new(System_Type type, size_t size) {
    // allocate the object
    System_Object o = malloc(size);
    memset(o, 0, size); // this is necessary now, idk why
    o->color = 0;

    // set the object type
    if (type != NULL) {
        ASSERT(type->VTable != NULL);
        o->vtable = type->VTable;
    }

    // if there is no finalize then always suppress the finalizer
    if (type != NULL) {
        o->suppress_finalizer = type->Finalize == NULL;
    }

    return o;
}

static void write_field(void* o, size_t offset, void* new) {
    *(void**)((uintptr_t)o + offset) = new;
}

void gc_update(void* o, size_t offset, void* new) {
    write_field(o, offset, new);
}
void gc_add_root() {}
void gc_update_ref(void* ptr, void* new) {
    //System_Object object = heap_find((uintptr_t)ptr);
    //if (object != NULL) {
        //gc_update(object, (uintptr_t)ptr - (uintptr_t)object, new);
    //} else {
        *((void**)ptr) = new;
    //}
}