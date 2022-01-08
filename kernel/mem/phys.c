#include <stdalign.h>
#include <util/string.h>
#include <sync/ticketlock.h>
#include <util/stb_ds.h>
#include "phys.h"
#include "early.h"
#include "mem.h"
#include "vmm.h"

/**
 * Code is based on:
 * https://github.com/spaskalev/buddy_alloc
 */

#define BUDDY_ALLOC_ALIGN           (sizeof(size_t) * 8)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Math utils
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static const unsigned char m_popcount_lookup[16] = {0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4};

static unsigned int popcount_byte(unsigned char b) {
    return m_popcount_lookup[b & 15] + m_popcount_lookup[b >> 4];
}

static size_t highest_bit_position(size_t value) {
    return (value == 0) ? 0 : 64 - __builtin_clzl(value);
}

static size_t ceiling_power_of_two(size_t value) {
    value += !value;
    return 1ull << (highest_bit_position(value + value - 1) - 1);
}

static size_t size_for_order(uint8_t order, uint8_t to) {
    size_t result = 0;
    size_t multi = 1u;

    while (order != to) {
        result += order * multi;
        order--;
        multi *= 2;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Bitset manipulation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define CHAR_BIT 8

size_t bitset_sizeof(size_t elements) {
    return ((elements) + CHAR_BIT - 1u) / CHAR_BIT;
}

static uint8_t m_bitset_index_mask[8] = {1, 2, 4, 8, 16, 32, 64, 128};

static void bitset_set(uint8_t* bitset, size_t pos) {
    size_t bucket = pos / CHAR_BIT;
    size_t index = pos % CHAR_BIT;
    bitset[bucket] |= m_bitset_index_mask[index];
}

static void bitset_clear(uint8_t* bitset, size_t pos) {
    size_t bucket = pos / CHAR_BIT;
    size_t index = pos % CHAR_BIT;
    bitset[bucket] &= ~m_bitset_index_mask[index];
}

static unsigned int bitset_test(const uint8_t* bitset, size_t pos) {
    size_t bucket = pos / CHAR_BIT;
    size_t index = pos % CHAR_BIT;
    return bitset[bucket] & m_bitset_index_mask[index];
}

static const uint8_t bitset_char_mask[8][8] = {
    {1, 3, 7, 15, 31, 63, 127, 255},
    {0, 2, 6, 14, 30, 62, 126, 254},
    {0, 0, 4, 12, 28, 60, 124, 252},
    {0, 0, 0,  8, 24, 56, 120, 248},
    {0, 0, 0,  0, 16, 48, 112, 240},
    {0, 0, 0,  0,  0, 32,  96, 224},
    {0, 0, 0,  0,  0,  0,  64, 192},
    {0, 0, 0,  0,  0,  0,   0, 128},
};

static void bitset_clear_range(uint8_t* bitset, size_t from_pos, size_t to_pos) {
    size_t from_bucket = from_pos / CHAR_BIT;
    size_t to_bucket = to_pos / CHAR_BIT;

    size_t from_index = from_pos % CHAR_BIT;
    size_t to_index = to_pos % CHAR_BIT;

    if (from_bucket == to_bucket) {
        bitset[from_bucket] &= ~bitset_char_mask[from_index][to_index];
    } else {
        bitset[from_bucket] &= ~bitset_char_mask[from_index][7];
        bitset[to_bucket] &= ~bitset_char_mask[0][to_index];

        while(++from_bucket != to_bucket) {
            bitset[from_bucket] = 0;
        }
    }
}

static void bitset_set_range(uint8_t* bitset, size_t from_pos, size_t to_pos) {
    size_t from_bucket = from_pos / CHAR_BIT;
    size_t to_bucket = to_pos / CHAR_BIT;

    size_t from_index = from_pos % CHAR_BIT;
    size_t to_index = to_pos % CHAR_BIT;

    if (from_bucket == to_bucket) {
        bitset[from_bucket] |= bitset_char_mask[from_index][to_index];
    } else {
        bitset[from_bucket] |= bitset_char_mask[from_index][7];
        bitset[to_bucket] |= bitset_char_mask[0][to_index];

        while(++from_bucket != to_bucket) {
            bitset[from_bucket] = 255u;
        }
    }
}

static size_t bitset_count_range(uint8_t* bitset, size_t from_pos, size_t to_pos) {
    size_t from_bucket = from_pos / CHAR_BIT;
    size_t to_bucket = to_pos / CHAR_BIT;

    size_t from_index = from_pos % CHAR_BIT;
    size_t to_index = to_pos % CHAR_BIT;

    if (from_bucket == to_bucket) {
        return popcount_byte(bitset[from_bucket] & bitset_char_mask[from_index][to_index]);
    }

    size_t result = popcount_byte(bitset[from_bucket] & bitset_char_mask[from_index][7]) +
                    popcount_byte(bitset[to_bucket]  & bitset_char_mask[0][to_index]);

    while(++from_bucket != to_bucket) {
        result += popcount_byte(bitset[from_bucket]);
    }

    return result;
}

static void bitset_shift_left(uint8_t* bitset, size_t from_pos, size_t to_pos, size_t by) {
    size_t length = to_pos - from_pos;
    for(size_t i = 0; i < length; i++) {
        size_t at = from_pos + i;
        if (bitset_test(bitset, at)) {
            bitset_set(bitset, at - by);
        } else {
            bitset_clear(bitset, at - by);
        }
    }
    bitset_clear_range(bitset, length, length + by - 1);

}

static void bitset_shift_right(uint8_t* bitset, size_t from_pos, size_t to_pos, size_t by) {
    intptr_t length = to_pos - from_pos;
    while (length >= 0) {
        size_t at = from_pos + length;
        if (bitset_test(bitset, at)) {
            bitset_set(bitset, at + by);
        } else {
            bitset_clear(bitset, at + by);
        }
        length -= 1;
    }
    bitset_clear_range(bitset, from_pos, from_pos+by-1);
}

static void bitset_debug(uint8_t* bitset, size_t length) {
    for (size_t i = 0; i < length; i++) {
        TRACE("%zu: %d", i, bitset_test(bitset, i) && 1);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Buddy tree manipulation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define BUDDY_TREE ((size_t*)BUDDY_TREE_START)
#define BUDDY_TREE_BITS ((uint8_t*)BUDDY_TREE_START)

/**
 *
 */
static uint8_t m_buddy_tree_order;

/**
 *
 */
static size_t m_buddy_tree_upper_pos_bound;

/**
 *
 */
static size_t m_buddy_tree_size_for_order_offset;

typedef struct buddy_tree_pos {
    size_t index;
    size_t depth;
} buddy_tree_pos_t;

#define INVALID_POS ((struct buddy_tree_pos){ 0, 0 })

static size_t buddy_tree_depth(buddy_tree_pos_t pos) {
    return pos.depth;
}

static size_t buddy_tree_order_for_memory(size_t memory_size) {
    size_t blocks = memory_size / BUDDY_ALLOC_ALIGN;
    return highest_bit_position(ceiling_power_of_two(blocks));
}

/**
 * Get the size of the buddy allocation tree of the desired order
 *
 * @param order [IN] The order of the tree we want
 */
static size_t buddy_tree_sizeof(uint8_t order) {
    /* Account for the bitset */
    size_t bitset_size = bitset_sizeof(size_for_order(order, 0));
    if (bitset_size % sizeof(size_t)) {
        bitset_size += (bitset_size % sizeof(size_t));
    }
    /* Account for the size_for_order memoization */
    size_t size_for_order_size = ((order + 2) * sizeof(size_t));

    return bitset_size + size_for_order_size;
}

/**
 * Get the root position of the buddy allocation tree
 */
static buddy_tree_pos_t buddy_tree_root() {
    return (buddy_tree_pos_t) {
        .index = 1,
        .depth = 1
    };
}

static buddy_tree_pos_t buddy_tree_leftmost_child_internal(size_t tree_order) {
    return (buddy_tree_pos_t) {
        .index = 1 << (tree_order - 1),
        .depth = tree_order
    };
}

static buddy_tree_pos_t buddy_tree_leftmost_child() {
    return buddy_tree_leftmost_child_internal(m_buddy_tree_order);
}

static buddy_tree_pos_t buddy_tree_left_child(buddy_tree_pos_t pos) {
    pos.index *= 2;
    pos.depth++;
    return pos;
}

static buddy_tree_pos_t buddy_tree_right_child(buddy_tree_pos_t pos) {
    pos.index *= 2;
    pos.index++;
    pos.depth++;
    return pos;
}

static buddy_tree_pos_t buddy_tree_sibling(buddy_tree_pos_t pos) {
    pos.index ^= 1;
    return pos;
}

static buddy_tree_pos_t buddy_tree_parent(buddy_tree_pos_t pos) {
    pos.index /= 2;
    pos.depth--;
    return pos;
}

static buddy_tree_pos_t buddy_tree_right_adjacent(buddy_tree_pos_t pos) {
    if (((pos.index + 1) ^ pos.index) > pos.index) {
        return INVALID_POS;
    }
    pos.index++;
    return pos;
}

static size_t buddy_tree_index_internal(buddy_tree_pos_t pos) {
    // Clear out the highest bit, this gives us the index
    // in a row of sibling nodes
    size_t mask = 1u << (pos.depth - 1) % ((sizeof(size_t) * CHAR_BIT) - 1);
    size_t result = pos.index & ~mask;
    return result;
}

static size_t buddy_tree_index(buddy_tree_pos_t pos) {
    return buddy_tree_index_internal(pos);
}

static void buddy_tree_populate_size_for_order() {
    size_t bitset_offset = bitset_sizeof(size_for_order(m_buddy_tree_order, 0));
    if (bitset_offset % sizeof(size_t)) {
        bitset_offset += (bitset_offset % sizeof(size_t));
    }
    m_buddy_tree_size_for_order_offset = bitset_offset / sizeof(size_t);
    m_buddy_tree_size_for_order_offset++;
    for (size_t i = 0; i <= m_buddy_tree_order; i++) {
        BUDDY_TREE[m_buddy_tree_size_for_order_offset + i] = size_for_order(m_buddy_tree_order, i);
    }
}

static size_t buddy_tree_size_for_order(uint8_t to) {
    return BUDDY_TREE[m_buddy_tree_size_for_order_offset + to];
}

typedef struct internal_position {
    size_t local_offset;
    size_t bitset_location;
} internal_position_t;

static internal_position_t buddy_tree_internal_position_tree(buddy_tree_pos_t pos) {
    internal_position_t p = {0};
    p.local_offset = m_buddy_tree_order - buddy_tree_depth(pos) + 1;
    size_t total_offset = buddy_tree_size_for_order(p.local_offset);
    size_t local_index = buddy_tree_index_internal(pos);
    p.bitset_location = total_offset + (p.local_offset * local_index);
    return p;
}

static void write_to_internal_position(internal_position_t pos, size_t value) {
    if (!value) {
        bitset_clear(BUDDY_TREE_BITS, pos.bitset_location);
        return;
    }
    bitset_clear_range(BUDDY_TREE_BITS, pos.bitset_location, pos.bitset_location + pos.local_offset - 1);
    bitset_set_range(BUDDY_TREE_BITS, pos.bitset_location, pos.bitset_location + value - 1);
}

static size_t read_from_internal_position(internal_position_t pos) {
    if (!bitset_test(BUDDY_TREE_BITS, pos.bitset_location)) {
        // fast path
        return 0;
    }
    return bitset_count_range(BUDDY_TREE_BITS, pos.bitset_location, pos.bitset_location + pos.local_offset - 1);
}


static size_t buddy_tree_status(buddy_tree_pos_t pos) {
    struct internal_position internal = buddy_tree_internal_position_tree(pos);
    return read_from_internal_position(internal);
}

static void update_parent_chain(buddy_tree_pos_t pos, internal_position_t pos_internal, size_t size_current) {
    while (pos.index != 1) {
        pos_internal.bitset_location += pos_internal.local_offset - (2 * pos_internal.local_offset * (pos.index & 1u));
        size_t size_sibling = read_from_internal_position(pos_internal);

        pos = buddy_tree_parent(pos);
        pos_internal = buddy_tree_internal_position_tree(pos);
        size_t size_parent = read_from_internal_position(pos_internal);

        size_t target_parent = (size_current || size_sibling) * ((size_current <= size_sibling ? size_current : size_sibling) + 1);
        if (target_parent == size_parent) {
            return;
        }

        write_to_internal_position(pos_internal, target_parent);
        size_current = target_parent;
    };
}

static void buddy_tree_mark(buddy_tree_pos_t pos) {
    /* Calling mark on a used position is a bug in caller */
    internal_position_t internal = buddy_tree_internal_position_tree(pos);

    /* Mark the node as used */
    write_to_internal_position(internal, internal.local_offset);

    /* Update the tree upwards */
    update_parent_chain(pos, internal, internal.local_offset);
}

static void buddy_tree_release(buddy_tree_pos_t pos) {
    // Calling release on an unused or a partially-used position a bug in caller
    internal_position_t internal = buddy_tree_internal_position_tree(pos);

    if (read_from_internal_position(internal) != internal.local_offset) {
        return;
    }

    // Mark the node as unused
    write_to_internal_position(internal, 0);

    // Update the tree upwards
    update_parent_chain(pos, internal, 0);
}


static buddy_tree_pos_t buddy_tree_find_free(uint8_t target_depth) {
    ASSERT(target_depth <= m_buddy_tree_order);

    buddy_tree_pos_t start = buddy_tree_root();
    uint8_t target_status = target_depth - 1;
    size_t current_depth = buddy_tree_depth(start);
    size_t current_status = buddy_tree_status(start);
    while (1) {
        if (current_depth == target_depth) {
            return current_status == 0 ? start : INVALID_POS;
        }

        // check if there are positions available down the tree
        if (current_status > target_status) {
            return INVALID_POS;
        }

        // Advance criteria
        target_status -= 1;
        current_depth += 1;

        // Do an optimal fit followed by left-first fit
        buddy_tree_pos_t left_pos = buddy_tree_left_child(start);
        buddy_tree_pos_t right_pos = buddy_tree_sibling(left_pos);
        internal_position_t internal = buddy_tree_internal_position_tree(left_pos);
        size_t left_status = read_from_internal_position(internal);
        internal.bitset_location += internal.local_offset;
        size_t right_status = read_from_internal_position(internal);

        if (left_status > target_status) {
            // left branch is busy, pick right
            start = right_pos;
            current_status = right_status;
            continue;
        }

        if (right_status > target_status) {
            // right branch is busy, pick left
            start = left_pos;
            current_status = left_status;
            continue;
        }

        // Both branches are good, pick the more-used one
        if (left_status >= right_status) {
            start = left_pos;
            current_status = left_status;
        } else {
            start = right_pos;
            current_status = right_status;
        }
    }
}

static unsigned int buddy_tree_valid(buddy_tree_pos_t pos) {
    return pos.index && (pos.index < m_buddy_tree_upper_pos_bound);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Buddy manpulation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static size_t m_buddy_memory_size = 0;

static size_t m_buddy_virtual_slots = 0;

size_t buddy_sizeof(size_t memory_size) {
    ASSERT (memory_size >= BUDDY_ALLOC_ALIGN);
    return buddy_tree_sizeof(buddy_tree_order_for_memory(memory_size));
}

static size_t buddy_effective_memory_size() {
    return ceiling_power_of_two(m_buddy_memory_size);
}

static size_t depth_for_size(size_t requested_size) {
    if (requested_size < BUDDY_ALLOC_ALIGN) {
        requested_size = BUDDY_ALLOC_ALIGN;
    }

    size_t depth = 1;
    size_t effective_memory_size = buddy_effective_memory_size();
    while ((effective_memory_size / requested_size) >> 1u) {
        depth++;
        effective_memory_size >>= 1u;
    }

    return depth;
}

static size_t size_for_depth(size_t depth) {
    depth += !depth; /* Silences a clang warning about undefined right shift */
    return ceiling_power_of_two(m_buddy_memory_size) >> (depth-1);
}


static void buddy_toggle_virtual_slots() {
    size_t memory_size = m_buddy_memory_size;
    /* Mask/unmask the virtual space if memory is not a power of two */
    size_t effective_memory_size = buddy_effective_memory_size();
    if (effective_memory_size == memory_size) {
        /* Update the virtual slot count */
        m_buddy_virtual_slots = 0;
        return;
    }

    /* Get the area that we need to mask and pad it to alignment */
    /* Node memory size is already aligned to BUDDY_ALLOC_ALIGN */
    size_t delta = effective_memory_size - memory_size;

    /* Update the virtual slot count */
    m_buddy_virtual_slots = delta / BUDDY_ALLOC_ALIGN;

    buddy_tree_pos_t pos = buddy_tree_right_child(buddy_tree_root());
    while (delta) {
        size_t current_pos_size = size_for_depth(buddy_tree_depth(pos));
        if (delta == current_pos_size) {
            // toggle current pos
            buddy_tree_mark(pos);
            break;
        }
        if (delta <= (current_pos_size / 2)) {
            // re-run for right child
            pos = buddy_tree_right_child(pos);
            continue;
        } else {
            // toggle right child
            buddy_tree_mark(buddy_tree_right_child(pos));
            // reduce delta
            delta -= current_pos_size / 2;
            // re-run for left child
            pos = buddy_tree_left_child(pos);
            continue;
        }
    }
}

static size_t offset_for_position(buddy_tree_pos_t pos) {
    size_t block_size = size_for_depth(buddy_tree_depth(pos));
    size_t addr = block_size * buddy_tree_index(pos);
    return addr;
}

static void* address_for_position(buddy_tree_pos_t pos) {
    return PHYS_TO_DIRECT(offset_for_position(pos));
}

static struct buddy_tree_pos deepest_position_for_offset(size_t offset) {
    size_t index = offset / BUDDY_ALLOC_ALIGN;
    struct buddy_tree_pos pos = buddy_tree_leftmost_child();
    pos.index += index;
    return pos;
}

static buddy_tree_pos_t position_for_address(const void* addr) {
    /* Find the deepest position tracking this address */
    ptrdiff_t offset = DIRECT_TO_PHYS(addr);

    if (offset % BUDDY_ALLOC_ALIGN) {
        return INVALID_POS;
    }

    buddy_tree_pos_t pos = deepest_position_for_offset(offset);

    // Find the actual allocated position tracking this address
    while (buddy_tree_valid(pos) && !buddy_tree_status(pos)) {
        pos = buddy_tree_parent(pos);
    }

    if (address_for_position(pos) != addr) {
        return INVALID_POS;
    }

    return pos;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Initialization
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// there are some problems when we are trying to use the __builtin_memset because the compiler
// doesn't like we use random abs addresses
#undef memset
extern void* memset(void* dest, int val, size_t n);

/**
 * The lock to protect ourselves
 */
static ticketlock_t m_palloc_lock = INIT_TICKETLOCK();

static err_t mark_range(uintptr_t base, size_t length) {
    err_t err = NO_ERROR;

    while (length > 0) {
        // find the top most entry that can fit this
        buddy_tree_pos_t pos = INVALID_POS;
        buddy_tree_pos_t pos_to_try = deepest_position_for_offset(base);
        do {
            size_t pos_size = size_for_depth(pos_to_try.depth);
            if (offset_for_position(pos_to_try) != base) {
                break;
            }

            // this is exactly as much as we need, take it
            if (length == pos_size) {
                pos = pos_to_try;
                break;
            }

            if (length < pos_size) {
                // we need less than this, break
                break;
            } else {
                // we need more than this, its a valid option
                pos = pos_to_try;
            }

            // get the parent and try again
            pos_to_try = buddy_tree_parent(pos_to_try);
        } while (buddy_tree_valid(pos_to_try));

        CHECK(buddy_tree_valid(pos));

        // get the size and mark the position as used
        size_t current_size = size_for_depth(pos.depth);
        buddy_tree_mark(pos);

        base += current_size;
        length -= current_size;
    }

    CHECK(length == 0);

cleanup:
    return err;
}

/**
 * Mark all unusable entries
 */
static err_t mark_unusable_ranges(struct stivale2_struct_tag_memmap* memmap) {
    err_t err = NO_ERROR;

    uintptr_t last_usable_end = 0;
    for (int i = 0; i < memmap->entries; i++) {
        struct stivale2_mmap_entry* entry = &memmap->memmap[i];
        if (entry->type == STIVALE2_MMAP_BOOTLOADER_RECLAIMABLE || entry->type == STIVALE2_MMAP_USABLE) {
            // mark from the last usable range to this one
            CHECK_AND_RETHROW(mark_range(last_usable_end, entry->base - last_usable_end));

            // update the last usable range
            last_usable_end = entry->base + entry->length;
        }
    }

    // because we set the top address of the buddy to be the highest usable address
    // we know there is nothing to mark at the end of the buddy itself, so we can just
    // continue normally

cleanup:
    return err;
}

/**
 * Mark bootloader reclaim ranges, we mark them in a different step than the
 * mark unusable so we can reclaim the memory later on
 */
static err_t mark_bootloader_reclaim(struct stivale2_struct_tag_memmap* memmap) {
    err_t err = NO_ERROR;

    for (int i = 0; i < memmap->entries; i++) {
        struct stivale2_mmap_entry* entry = &memmap->memmap[i];
        if (entry->type == STIVALE2_MMAP_BOOTLOADER_RECLAIMABLE) {
            CHECK_AND_RETHROW(mark_range(entry->base, entry->length));
        }
    }

cleanup:
    return err;
}

err_t init_palloc() {
    err_t err = NO_ERROR;

    // figure the amount of
    struct stivale2_struct_tag_memmap* memmap = get_stivale2_tag(STIVALE2_STRUCT_TAG_MEMMAP_ID);
    CHECK(memmap != NULL);

    // find top address and align it to the buddy alignment,
    // we are only going to consider usable and reclaimable
    // addresses of course
    uintptr_t top_address = 0;
    for (int i = 0; i < memmap->entries; i++) {
        uintptr_t base = memmap->memmap[i].base;
        size_t length = memmap->memmap[i].length;
        int type = memmap->memmap[i].type;
        if (type == STIVALE2_MMAP_USABLE || type == STIVALE2_MMAP_BOOTLOADER_RECLAIMABLE) {
            if (base + length > top_address) {
                top_address = base + length;
            }
        }
    }

    // setup the global params for the buddy allocator
    m_buddy_memory_size = ALIGN_DOWN(top_address, BUDDY_ALLOC_ALIGN);
    size_t size = ALIGN_UP(buddy_sizeof(top_address), PAGE_SIZE);
    m_buddy_tree_order = buddy_tree_order_for_memory(top_address);
    m_buddy_tree_upper_pos_bound = 1 << m_buddy_tree_order;

    // validate the parameters
    CHECK(m_buddy_memory_size <= DIRECT_MAP_SIZE);
    CHECK(size <= BUDDY_TREE_SIZE);

    // map the whole buddy map, allocate it right now since we don't wanna mess too much with demand paging...
    CHECK_AND_RETHROW(vmm_alloc((void*)BUDDY_TREE_START, size / PAGE_SIZE, MAP_WRITE | MAP_UNMAP_DIRECT));
    memset((void*)BUDDY_TREE_START, 0, size);

    // Populate the size for order and setup the
    // virtual slots
    buddy_tree_populate_size_for_order();
    buddy_toggle_virtual_slots();

    // first we are going to mark the whole tree as used, this is
    // to make sure that nothing can be allocated
    CHECK_AND_RETHROW(mark_unusable_ranges(memmap));
    CHECK_AND_RETHROW(mark_bootloader_reclaim(memmap));

cleanup:
    return err;
}

err_t palloc_reclaim() {
    err_t err = NO_ERROR;
    struct stivale2_mmap_entry* to_reclaim = NULL;

    // gather and copy entries that we need (we must copy them
    // because otherwise we are going to reclaim them)
    struct stivale2_struct_tag_memmap* memmap = get_stivale2_tag(STIVALE2_STRUCT_TAG_MEMMAP_ID);
    CHECK(memmap != NULL);
    for (int i = 0; i < memmap->entries; i++) {
        if (memmap->memmap[i].type == STIVALE2_MMAP_BOOTLOADER_RECLAIMABLE) {
            arrpush(to_reclaim, memmap->memmap[i]);
        }
    }

    // now actually reclaim them
    TRACE("Reclaiming memory");
    for (int i = 0; i < arrlen(to_reclaim); i++) {
        struct stivale2_mmap_entry* entry = &to_reclaim[i];
        TRACE("\t%p-%p: %llu bytes", entry->base, entry->base + entry->length, entry->length);

        void* ptr = PHYS_TO_DIRECT(entry->base);
        size_t length = entry->length;
        while (length > 0) {
            // get and free the entry
            buddy_tree_pos_t pos = position_for_address(ptr);
            CHECK(buddy_tree_valid(pos));
            buddy_tree_release(pos);

            // advance to next one
            size_t size = size_for_depth(pos.depth);
            ptr += size;
            length -= size;
        }
    }

cleanup:
    arrfree(to_reclaim);
    return err;
}

void* palloc(size_t size) {
    // return valid pointer for size == 0
    if (size == 0) {
        size = 1;
    }

    // too big to handle
    if (size > m_buddy_memory_size) {
        return NULL;
    }

    ticketlock_lock(&m_palloc_lock);

    size_t target_depth = depth_for_size(size);
    buddy_tree_pos_t pos = buddy_tree_find_free(target_depth);
    if (!buddy_tree_valid(pos)) {
        // no slot was found
        ticketlock_unlock(&m_palloc_lock);
        return NULL;
    }

    // allocate this slot
    buddy_tree_mark(pos);

    // Get the actual pointer value
    void* ptr = address_for_position(pos);

    ticketlock_unlock(&m_palloc_lock);

    return ptr;
}

void pfree(void* base) {
    // handle base == NULL
    if (base == NULL) {
        return;
    }

    ticketlock_lock(&m_palloc_lock);

    buddy_tree_pos_t pos = position_for_address(base);
    ASSERT (buddy_tree_valid(pos));
    buddy_tree_release(pos);

    ticketlock_unlock(&m_palloc_lock);
}
