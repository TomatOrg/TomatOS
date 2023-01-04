#include "util/except.h"
#include <dotnet/gc/heap.h>

#include <dotnet/gc/gc.h>

#include <thread/cpu_local.h>
#include <thread/thread.h>
#include <arch/intrin.h>
#include <util/string.h>
#include <util/defs.h>
#include <mem/mem.h>
#include <debug/asan.h>

#include <kernel.h>

#include <stdatomic.h>

#include <mimalloc.h>
#include <mimalloc-internal.h>


void heap_dump_mapping() {}

System_Object heap_alloc(size_t size, int color) {
    mi_heap_t** heap = (mi_heap_t**)&(get_current_thread()->heap);
    if (*heap == NULL) {
        *heap = mi_heap_new();
    }
    System_Object object = (System_Object) mi_heap_zalloc(*heap, size);
    object->color = color;
    return object;
}

void heap_free(System_Object object) {
    // zero-out the entire object, this includes setting the color to zero, which will
    // essentially free the object, this can be done without locking at all because at worst
    // something is going to allocate it in a second
    object->color = COLOR_BLUE;
    mi_free(object);
}

void heap_reclaim() { }

System_Object heap_find_fast(void *ptr) {
    if ((uintptr_t)ptr < OBJECT_HEAP_START || (uintptr_t)ptr >= OBJECT_HEAP_END) return NULL;
    mi_segment_t* segment = _mi_ptr_segment(ptr);
    mi_page_t* page = _mi_segment_page_of(segment, ptr);
    return (void*)_mi_page_ptr_unalign(segment, page, ptr);
}

System_Object heap_find(uintptr_t p) {
    ASSERT(!"heap find called");
}

// work around an overzealous assert in mimalloc
// it checks that the element count before and after the traversal matches
// but this is at odd with most TDN heap walks (which allocate objects), and especially the GC 
static bool heap_visitor(const mi_heap_t* heap, const mi_heap_area_t* area, void* block, size_t block_size, void* arg) {
    if (!block) return true;
    System_Object** arr = arg;
    System_Object o = block;
    arrpush(*arr, o);
    return true;
}

void heap_iterate_objects(object_callback_t callback) {
    for (int j = 0; j < arrlen(g_all_threads); j++) {
        System_Object* arr = NULL;
        mi_heap_t* heap = g_all_threads[j]->heap;
        if (heap == NULL) continue;
        mi_heap_visit_blocks(heap, true, heap_visitor, &arr);
        for (int i = 0; i < arrlen(arr); i++) {
            if (callback) callback(arr[i]);
        }
        arrfree(arr);
    }
}

// TODO: actually only iterate through dirty objects
void heap_iterate_dirty_objects(object_callback_t callback) {
    if (callback) heap_iterate_objects(callback);
}

void heap_dump() {
}
