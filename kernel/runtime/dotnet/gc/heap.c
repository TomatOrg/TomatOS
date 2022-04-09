#include "heap.h"

#include "gc.h"

#include <threading/thread.h>
#include <util/string.h>
#include <util/defs.h>
#include <mem/mem.h>

#include <stdatomic.h>

#define UNIT 64
STATIC_ASSERT((UNIT >> 1) < sizeof(struct System_Object));
STATIC_ASSERT(UNIT >= sizeof(struct System_Object));

typedef struct heap_rank {
    _Atomic(System_Object) chunks;
    size_t freed;
} heap_rank_t;

/**
 * Each thread has its own rank, the head is used for two different stuff:
 * - on mutator threads it is used to have a list of ready to use allocations,
 *   the list is essentially a single chunk that is given to the thread
 * - on collector thread it is used to have a bunch of objects to form a chunk
 *   to then release to the allocator at once (for performance reasons)
 */
static THREAD_LOCAL System_Object m_rank_head[7] = {0 };

/**
 * The heaps are global to all the cores...
 */
static heap_rank_t m_heap_ranks[7];

//----------------------------------------------------------------------------------------------------------------------
// Allocation can happen from any thread, must be done atomically
//----------------------------------------------------------------------------------------------------------------------

/**
 * Grow the heap by allocating a new chunk for the given rank, the chunk is made of objects
 * following each other in a singly linked list
 */
static System_Object heap_rank_allocate_chunk(size_t rank, size_t count) {
    size_t size = UNIT << rank;
    size_t length = size * count;
    void* block = palloc(length);
    ASSERT(block != NULL);

    // go and set all the objects
    void* p = block;
    for (size_t i = 1; i < count; i++) {
        // set the object, including color, rank and the next
        // free object
        System_Object obj = p;
        obj->color = GC_COLOR_BLUE;
        obj->rank = rank;
        obj->next = (void*)(p += size);
    }

    // This is the last object, don't set a
    // next pointer
    System_Object obj = p;
    obj->color = GC_COLOR_BLUE;
    obj->rank = rank;

    // return the first item
    return block;
}

/**
 * Allocate an object from the heap, try to get a chunk that can be used
 * and if no chunk is available then allocate a new one
 */
static System_Object heap_rank_alloc(heap_rank_t* heap, size_t rank, size_t count) {
    // attempt to take a chunk from the existing heap
    System_Object p = atomic_load_explicit(&heap->chunks, memory_order_acquire);
    while (p != NULL && !atomic_compare_exchange_weak_explicit(&heap->chunks, &p, p->chunk_next, memory_order_acquire, memory_order_acquire));

    if (p == NULL) {
        // there is no existing chunk, allocate a new one
        p = heap_rank_allocate_chunk(rank, count);
    }

    // return the first object from the chunk
    return p;
}

//----------------------------------------------------------------------------------------------------------------------
// Only one thread ever frees objects, which is the collector thread, so no need to do any special synchronization here
//----------------------------------------------------------------------------------------------------------------------

/**
 * Returns all the freed objects on the given rank to the heap
 * as a single chunk, it is up to the caller to track the amount of
 */
static void heap_rank_return(heap_rank_t* heap, size_t rank) {
    // take the head and set the next chunk as the thing
    System_Object o = m_rank_head[rank];
    o->chunk_next = atomic_load_explicit(&heap->chunks, memory_order_relaxed);
    while (!atomic_compare_exchange_weak_explicit(&heap->chunks, &o->chunk_next, o, memory_order_relaxed, memory_order_relaxed));

    // no more items on this level
    m_rank_head[rank] = NULL;
}

/**
 * Flush all the freed items to the global heap rank
 */
static void heap_rank_flush(heap_rank_t* heap, size_t rank) {
    // return it
    heap_rank_return(heap, rank);

    // we don't have any free items now, no need for atomic operations since
    // only a single thread is taking care of this
    heap->freed = 0;
}

/**
 * Free a single object, queueing it to a chunk that will eventually be flushed to
 * the global rank
 */
static void heap_rank_free(heap_rank_t* heap, size_t rank, size_t count, System_Object obj) {
    // queue it
    obj->next = m_rank_head[rank];
    m_rank_head[rank] = obj;

    // catch use-after-free
    memset(obj, 0xCD, UNIT << rank);

    // check if we need to flush this as a single chunk
    if (++heap->freed >= count) {
        heap_rank_flush(heap, rank);
    }
}

//----------------------------------------------------------------------------------------------------------------------
// This takes on all the heap ranks and combines them
//----------------------------------------------------------------------------------------------------------------------

// This is just for easy documentation
#define HEAP_RANK_0_SIZE 64
#define HEAP_RANK_1_SIZE 128
#define HEAP_RANK_2_SIZE 256
#define HEAP_RANK_3_SIZE 512
#define HEAP_RANK_4_SIZE 1024
#define HEAP_RANK_5_SIZE 2048
#define HEAP_RANK_6_SIZE 4096

STATIC_ASSERT(HEAP_RANK_0_SIZE == UNIT << 0);
STATIC_ASSERT(HEAP_RANK_1_SIZE == UNIT << 1);
STATIC_ASSERT(HEAP_RANK_2_SIZE == UNIT << 2);
STATIC_ASSERT(HEAP_RANK_3_SIZE == UNIT << 3);
STATIC_ASSERT(HEAP_RANK_4_SIZE == UNIT << 4);
STATIC_ASSERT(HEAP_RANK_5_SIZE == UNIT << 5);
STATIC_ASSERT(HEAP_RANK_6_SIZE == UNIT << 6);

//
// The amount of objects allocated on each rank, we assume smaller
// objects are going to be allocated more frequently than large objects
// very large objects are not going to be allocated using the normal list,
// but by allocating directly from the buddy allocator
//
#define HEAP_RANK_0_COUNT (1024 * 64)
#define HEAP_RANK_1_COUNT (1024 * 16)
#define HEAP_RANK_2_COUNT (1024 * 4)
#define HEAP_RANK_3_COUNT (1024)
#define HEAP_RANK_4_COUNT (1024 / 4)
#define HEAP_RANK_5_COUNT (1024 / 16)
#define HEAP_RANK_6_COUNT (1024 / 64)

/**
 * Allocate a chunk from the global heap rank, allocating the first object and
 * saving the next one in the local heap head
 *
 * @param heap_rank     [IN] The heap rank structure
 * @param rank          [IN] The rank of this object
 * @param count         [IN] The count
 */
static System_Object heap_allocate_from(heap_rank_t* heap_rank, size_t rank, size_t count) {
    System_Object p = heap_rank_alloc(heap_rank, rank, count);
    m_rank_head[rank] = p->next;
    return p;
}

/**
 * Allocate an object from the heap, if there is one available locally take it, otherwise
 * allocate a new one from the global heap
 */
static System_Object heap_allocate(heap_rank_t* heap_rank, size_t rank, size_t count) {
    System_Object p = m_rank_head[rank];
    if (p == NULL) {
        return heap_allocate_from(heap_rank, rank, count);
    }
    m_rank_head[rank] = p->next;
    return p;
}

// for easier access to each of the levels
static System_Object heap_allocate_0() { return heap_allocate(&m_heap_ranks[0], 0, HEAP_RANK_0_COUNT); }
static System_Object heap_allocate_1() { return heap_allocate(&m_heap_ranks[1], 1, HEAP_RANK_1_COUNT); }
static System_Object heap_allocate_2() { return heap_allocate(&m_heap_ranks[2], 2, HEAP_RANK_2_COUNT); }
static System_Object heap_allocate_3() { return heap_allocate(&m_heap_ranks[3], 3, HEAP_RANK_3_COUNT); }
static System_Object heap_allocate_4() { return heap_allocate(&m_heap_ranks[4], 4, HEAP_RANK_4_COUNT); }
static System_Object heap_allocate_5() { return heap_allocate(&m_heap_ranks[5], 5, HEAP_RANK_5_COUNT); }
static System_Object heap_allocate_6() { return heap_allocate(&m_heap_ranks[6], 6, HEAP_RANK_6_COUNT); }

/**
 * Allocate a large object, used when the object is larger than any
 * of the pre-made heaps
 */
static System_Object heap_allocate_large(size_t size) {
    System_Object o = palloc(size);
    ASSERT(o != NULL);
    o->rank = -1;
    return o;
}

/**
 * Allocate a medium object, one which doesn't fit to the first level heap
 */
static System_Object heap_allocate_medium(size_t size) {
    if (size < HEAP_RANK_1_SIZE) return heap_allocate_1();
    if (size < HEAP_RANK_2_SIZE) return heap_allocate_2();
    if (size < HEAP_RANK_3_SIZE) return heap_allocate_3();
    if (size < HEAP_RANK_4_SIZE) return heap_allocate_4();
    if (size < HEAP_RANK_5_SIZE) return heap_allocate_5();
    if (size < HEAP_RANK_6_SIZE) return heap_allocate_6();
    return heap_allocate_large(size);
}

static atomic_size_t m_heap_alive = 0;

System_Object heap_alloc(size_t size) {
    m_heap_alive++;

    // fast path for small objects
    if (size < HEAP_RANK_0_SIZE) {
        return heap_allocate_0();
    }
    return heap_allocate_medium(size);
}

void heap_free(System_Object object) {
    m_heap_alive--;

    switch (object->rank) {
        case 0: heap_rank_free(&m_heap_ranks[0], 0, HEAP_RANK_0_COUNT, object); break;
        case 1: heap_rank_free(&m_heap_ranks[1], 1, HEAP_RANK_1_COUNT, object); break;
        case 2: heap_rank_free(&m_heap_ranks[2], 2, HEAP_RANK_2_COUNT, object); break;
        case 3: heap_rank_free(&m_heap_ranks[3], 3, HEAP_RANK_3_COUNT, object); break;
        case 4: heap_rank_free(&m_heap_ranks[4], 4, HEAP_RANK_4_COUNT, object); break;
        case 5: heap_rank_free(&m_heap_ranks[5], 5, HEAP_RANK_5_COUNT, object); break;
        case 6: heap_rank_free(&m_heap_ranks[6], 6, HEAP_RANK_6_COUNT, object); break;
        default: {
            pfree(object);
        } break;
    }
}

void heap_flush() {
    heap_rank_flush(&m_heap_ranks[0], 0);
    heap_rank_flush(&m_heap_ranks[1], 1);
    heap_rank_flush(&m_heap_ranks[2], 2);
    heap_rank_flush(&m_heap_ranks[3], 3);
    heap_rank_flush(&m_heap_ranks[4], 4);
    heap_rank_flush(&m_heap_ranks[5], 5);
    heap_rank_flush(&m_heap_ranks[6], 6);
}

size_t heap_alive() {
    return m_heap_alive;
}
