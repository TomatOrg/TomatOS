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

static mi_heap_t* m_heap;

err_t init_heap() {
    void mi_process_load();
    mi_process_load();
    m_heap = mi_heap_new();
    return NO_ERROR;
}

void heap_dump_mapping() {}

System_Object heap_alloc(size_t size, int color) {
    System_Object object = (System_Object) mi_heap_zalloc(m_heap, size);
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
    // TODO:
    if ((uintptr_t)ptr < 0xFFFF810000000000ULL) {
        return NULL;
    }
    //printf("heap find fast %p\n", ptr);
    mi_segment_t* segment = _mi_ptr_segment(ptr);
    mi_page_t* page = _mi_segment_page_of(segment, ptr);
    //uintptr_t page_start = (uintptr_t)_mi_segment_page_start(segment, page, page->xblock_size, NULL, NULL);
    //uintptr_t diff = ((uintptr_t)ptr - page_start) % page->xblock_size;
    //uintptr_t start = (uintptr_t)ptr - diff;
    return (void*)_mi_page_ptr_unalign(segment, page, ptr);
}

System_Object heap_find(uintptr_t p) {
    ASSERT(!"heap find called");
}

static bool heap_visitor(const mi_heap_t* heap, const mi_heap_area_t* area, void* block, size_t block_size, void* arg) {
    if (!block) return true;
    object_callback_t callback = arg;
    System_Object o = block;
    callback(o);
    return true;
}

void heap_iterate_objects(object_callback_t callback) {
    mi_heap_visit_blocks(m_heap, true, heap_visitor, callback);
}

// TODO: actually only iterate through dirty objects
void heap_iterate_dirty_objects(object_callback_t callback) {
    if (callback) heap_iterate_objects(callback);
}

void heap_dump() {
}
