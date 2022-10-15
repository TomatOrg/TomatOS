#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <debug/asan.h>
#include "tlsf.h"

#if HAVE_CONFIG_H
#include "config.h"
#endif

/*
 * Architecture-specific bit manipulation routines
 *
 * TLSF achieves O(1) cost for malloc and free operations by limiting
 * the search for a free block to a free list of guaranteed size
 * adequate to fulfill the request, combined with efficient free list
 * queries using bitmasks and architecture-specific bit-manipulation
 * routines.
 *
 * Most modern processors provide instructions to count leading zeroes
 * in a word, find the lowest and highest set bit, etc. These
 * specific implementations will be used when available, falling back
 * to a reasonably efficient generic implementation.
 *
 * NOTE: TLSF spec relies on ffs/fls returning value 0..31.
 * ffs/fls return 1-32 by default, returning 0 for error.
 */

#if !defined (__x86_64__)
#error "unsupported architecture, only x86_64 supported"
#endif

static int tlsf_ffsll(long long word)
{
	return __builtin_ffsll(word) - 1;
}

static int tlsf_fls_sizet(size_t word)
{
	const int ull_bits = 8 * sizeof(unsigned long long);
	const int bit = word ? ull_bits - __builtin_clzll((unsigned long long)word) : 0;
	return bit - 1;
}

/*
 * Constants.
 */

/* Public constants: may be modified */
enum tlsf_public {
	/* log2 of number of linear subdivisions of block sizes. Larger
	 * values require more memory in the control structure. Values of
	 * 4 or 5 are typical.
	 */
	SL_INDEX_COUNT_LOG2 = 5,
};

/* Private constants: do not modify */
enum tlsf_private {
	/* All allocation sizes and addresses are aligned to 8 bytes */
	ALIGN_SIZE_LOG2 = 3,
	ALIGN_SIZE = (1 << ALIGN_SIZE_LOG2),

	/*
	 * We support allocations of sizes up to (1 << FL_INDEX_MAX) bits.
	 * However, because we linearly subdivide the second-level lists, and
	 * our minimum size granularity is 4 bytes, it doesn't make sense to
	 * create first-level lists for sizes smaller than SL_INDEX_COUNT * 4,
	 * or (1 << (SL_INDEX_COUNT_LOG2 + 2)) bytes, as there we will be
	 * trying to split size ranges into more slots than we have available.
	 * Instead, we calculate the minimum threshold size, and place all
	 * blocks below that size into the 0th first-level list.
	 */

	/*
	 * Increased this (from 32 to 40) to support larger sizes, at the expense
	 * of more overhead in the TLSF structure.
	 */
	FL_INDEX_MAX = 40,
	SL_INDEX_COUNT = (1 << SL_INDEX_COUNT_LOG2),
	FL_INDEX_SHIFT = (SL_INDEX_COUNT_LOG2 + ALIGN_SIZE_LOG2),
	FL_INDEX_COUNT = (FL_INDEX_MAX - FL_INDEX_SHIFT + 1),

	SMALL_BLOCK_SIZE = (1 << FL_INDEX_SHIFT),
};

/*
 * Cast and min/max macros.
 */

#define tlsf_cast(t, exp)       ((t)(exp))
#define tlsf_min(a, b)          ((a) < (b) ? (a) : (b))
#define tlsf_max(a, b)          ((a) > (b) ? (a) : (b))

/*
 * Set assert macro, if it has not been provided by the user.
 */
#if !defined (tlsf_assert)
#define tlsf_assert assert
#endif

/*
 * Static assertion mechanism.
 */

#define _tlsf_glue2(x, y) x ## y
#define _tlsf_glue(x, y) _tlsf_glue2(x, y)
#define tlsf_static_assert(exp) \
	typedef char _tlsf_glue (static_assert, __LINE__) [(exp) ? 1 : -1]

/* This code has been tested on 32- and 64-bit (LP/LLP) architectures */
tlsf_static_assert(sizeof(int) * CHAR_BIT == 32);
tlsf_static_assert(sizeof(size_t) * CHAR_BIT >= 32);
tlsf_static_assert(sizeof(size_t) * CHAR_BIT <= 64);

/* Ensure we've properly tuned our sizes */
tlsf_static_assert(ALIGN_SIZE == SMALL_BLOCK_SIZE / SL_INDEX_COUNT);

/*
 * Data structures and associated constants.
 */

/*
 * Block header structure.
 *
 * There are several implementation subtleties involved:
 * - The prev_phys_block field is only valid if the previous block is free.
 * - The prev_phys_block field is actually stored at the end of the
 *   previous block. It appears at the beginning of this structure only to
 *   simplify the implementation.
 * - The next_free / prev_free fields are only valid if the block is free.
 */
typedef struct block_header {
	/* Trailer of previous block */
	struct trailer {
		/* Points to the previous physical block */
		struct block_header *prev_phys_block;
	} prev_trailer;

	struct metadata {
		/* The size of this block, excluding the block header.
		 * The last two bits are used for flags.
		 */
		size_t size;

		/* The heap this allocation belongs to */
		tlsf_t *tlsf;
#ifndef DNDEBUG
		/* Who allocated it? */
		void *allocation_addr;
#endif
	} metadata;

	struct free_list {
		/* Next and previous free blocks */
		struct block_header *next_free;
		struct block_header *prev_free;
	} free_list;
} block_header_t;

/*
 * Since block sizes are always at least a multiple of 4, the two least
 * significant bits of the size field are used to store the block status:
 * - bit 0: whether block is busy or free
 * - bit 1: whether previous block is busy or free
 */
static const size_t block_header_free_bit = 1 << 0;
static const size_t block_header_prev_free_bit = 1 << 1;

/*
 * The size of the block header exposed to used blocks is the size field.
 * The prev_phys_block field is stored *inside* the previous free block.
 */
static const size_t metadata_size = sizeof(struct metadata);

/*
 * The size of the block header that overlaps the previous block,
 * namely the size of prev_phys_block field.
 */
static const size_t trailer_size = sizeof(struct trailer);


/* User data starts directly after the metadata record in a used block */
static const size_t block_start_offset = offsetof(block_header_t, free_list);

/*
 * A free block must be large enough to store its header minus the size of
 * the metadata, and no larger than the number of addressable
 * bits for FL_INDEX.
 */
static const size_t block_size_min = sizeof(block_header_t) - sizeof(struct metadata);
static const size_t block_size_max = tlsf_cast(size_t, 1) << FL_INDEX_MAX;


/* The TLSF control structure */
struct tlsf {
	/* Empty lists point at this block to indicate they are free */
	block_header_t block_null;

	/* Bitmaps for free lists */
	uint64_t fl_bitmap;
	uint64_t sl_bitmap[FL_INDEX_COUNT];

	/* Head of free lists */
	block_header_t *blocks[FL_INDEX_COUNT][SL_INDEX_COUNT];
};

/* SL_INDEX_COUNT must be <= number of bits in sl_bitmap's storage type */
tlsf_static_assert(sizeof(uint64_t) * CHAR_BIT >= SL_INDEX_COUNT);

/*
 * block_header_t member functions.
 */

static size_t block_size(const block_header_t *block)
{
	return block->metadata.size & ~(block_header_free_bit | block_header_prev_free_bit);
}

static void block_set_size(block_header_t *block, size_t size)
{
	const size_t oldsize = block->metadata.size;
	block->metadata.size = size | (oldsize & (block_header_free_bit | block_header_prev_free_bit));
}

static int block_is_last(const block_header_t *block)
{
	return block_size(block) == 0;
}

static int block_is_free(const block_header_t *block)
{
	return tlsf_cast(int, block->metadata.size & block_header_free_bit);
}

static void block_set_free(block_header_t *block)
{
	block->metadata.size |= block_header_free_bit;
}

static void block_set_used(block_header_t *block)
{
	block->metadata.size &= ~block_header_free_bit;
}

static int block_is_prev_free(const block_header_t *block)
{
	return tlsf_cast(int, block->metadata.size & block_header_prev_free_bit);
}

static void block_set_prev_free(block_header_t *block)
{
	block->metadata.size |= block_header_prev_free_bit;
}

static void block_set_prev_used(block_header_t *block)
{
	block->metadata.size &= ~block_header_prev_free_bit;
}

static block_header_t *block_from_ptr(const void *ptr)
{
	return tlsf_cast(block_header_t *,
		tlsf_cast(unsigned char *, ptr) - block_start_offset);
}

static void *block_to_ptr(const block_header_t *block)
{
	return tlsf_cast(void *,
		tlsf_cast(unsigned char *, block) + block_start_offset);
}

/* Return first block of pool */
static block_header_t *first_block(const void *ptr)
{
	return tlsf_cast(block_header_t *, tlsf_cast(ptrdiff_t, ptr) - trailer_size);
}

/* Return location of previous block */
// ASAN pre: unpoisoned metadata: block
// ASAN temporarily unpoisons trailer
static block_header_t *block_prev(const block_header_t *block)
{
	tlsf_assert(block_is_prev_free(block) && "previous block must be free");
	ASAN_UNPOISON_MEMORY_REGION(&block->prev_trailer, sizeof(struct trailer));
	block_header_t *prev = block->prev_trailer.prev_phys_block;
	ASAN_POISON_MEMORY_REGION(&block->prev_trailer, sizeof(struct trailer));
	return prev;
}

/* Return location of next existing block */
static block_header_t *block_next(const block_header_t *block)
{
	size_t size = block_size(block);
	block_header_t *next = tlsf_cast(block_header_t *, tlsf_cast(ptrdiff_t, block) + size + metadata_size);
	tlsf_assert(!block_is_last(block));
	return next;
}

/* Link a new block with its physical neighbor */
// ASAN pre: unpoisoned metadata: block
static void block_link_next(block_header_t *block)
{
	block_header_t *next = block_next(block);
	ASAN_UNPOISON_MEMORY_REGION(&next->prev_trailer, sizeof(struct trailer));
	next->prev_trailer.prev_phys_block = block;
	ASAN_POISON_MEMORY_REGION(&next->prev_trailer, sizeof(struct trailer));
}

// ASAN epxects unpoisoned metadata block
// ASAN leaves it unpoisoned
// ASAN temporarily unpoisons next block
static void block_mark_as_free(block_header_t *block)
{
	/* Link the block to the next block, first */
	block_link_next(block);
	block_header_t *next = block_next(block);
	ASAN_UNPOISON_MEMORY_REGION(&next->metadata, sizeof(struct metadata));
	block_set_prev_free(next);
	ASAN_POISON_MEMORY_REGION(&next->metadata, sizeof(struct metadata));
	block_set_free(block);
}

// ASAN pre: unpoisoned metadata block
// ASAN post: unpoisoned metadata block
// ASAN temporarily unpoisons next block
static void block_mark_as_used(block_header_t *block)
{
	block_header_t *next = block_next(block);
	ASAN_UNPOISON_MEMORY_REGION(&next->metadata, sizeof(struct metadata));
	block_set_prev_used(next);
	ASAN_POISON_MEMORY_REGION(&next->metadata, sizeof(struct metadata));
	block_set_used(block);
}

static size_t align_up(size_t x, size_t align)
{
	tlsf_assert(0 == (align & (align - 1)) && "must align to a power of two");
	return (x + (align - 1)) & ~(align - 1);
}

static size_t align_down(size_t x, size_t align)
{
	tlsf_assert(0 == (align & (align - 1)) && "must align to a power of two");
	return x - (x & (align - 1));
}

static void *align_ptr(const void *ptr, size_t align)
{
	const ptrdiff_t aligned =
		(tlsf_cast(ptrdiff_t, ptr) + (align - 1)) & ~(align - 1);
	tlsf_assert(0 == (align & (align - 1)) && "must align to a power of two");
	return tlsf_cast(void *, aligned);
}

/*
 * Adjust an allocation size to be aligned to word size, and no smaller
 * than internal minimum.
 */
static size_t adjust_request_size(size_t size, size_t align)
{
	size_t adjust = 0;
	if (size > 0) {
		const size_t aligned = align_up(size, align);

		/* aligned sized must not exceed block_size_max or we'll go out of bounds on sl_bitmap */
		if (aligned < block_size_max) {
			adjust = tlsf_max(aligned, block_size_min);
		}
	}
	return adjust;
}

/*
 * TLSF utility functions. In most cases, these are direct translations of
 * the documentation found in the white paper.
 */

static void mapping_search(size_t size, int *fli, int *sli)
{
	int fl;
	int sl;
	if (size < SMALL_BLOCK_SIZE) {
		/* Store small blocks in first list */
		fl = 0;
		sl = tlsf_cast(int, size) / (SMALL_BLOCK_SIZE / SL_INDEX_COUNT);
	} else {
		fl = tlsf_fls_sizet(size);
		sl = tlsf_cast(int, size >> (fl - SL_INDEX_COUNT_LOG2)) ^ (1 << SL_INDEX_COUNT_LOG2);
		fl -= (FL_INDEX_SHIFT - 1);
	}
	*fli = fl;
	*sli = sl;
}

static block_header_t *search_suitable_block(tlsf_t *tlsf, int *fli, int *sli)
{
	int fl = *fli;
	int sl = *sli;

	/*
	 * First, search for a block in the list associated with the given
	 * fl/sl index.
	 */
	uint64_t sl_map = tlsf->sl_bitmap[fl] & (~UINT64_C(0) << sl);
	if (sl_map == 0) {
		/* No block exists. Search in the next largest first-level list */
		uint64_t fl_map = tlsf->fl_bitmap & (~UINT64_C(0) << (fl + 1));
		if (fl_map == 0) {
			/* No free blocks available, memory has been exhausted */
			return NULL;
		}

		fl = tlsf_ffsll(fl_map);
		*fli = fl;
		sl_map = tlsf->sl_bitmap[fl];
	}
	tlsf_assert(sl_map && "internal error - second level bitmap is null");
	sl = tlsf_ffsll(sl_map);
	*sli = sl;

	/* Return the first block in the free list */
	block_header_t *block = tlsf->blocks[fl][sl];

	ASAN_UNPOISON_MEMORY_REGION(&block->metadata, sizeof(struct metadata));

	return block;
}

/* Remove a free block from the free list */
// ASAN pre: unpoisoned metadata block
// ASAN temporarily unpoisons block/next/prev free list
static void remove_free_block(tlsf_t *tlsf, block_header_t *block, int fl, int sl)
{
	ASAN_UNPOISON_MEMORY_REGION(&block->free_list, sizeof(struct free_list));
	block_header_t *prev = block->free_list.prev_free;
	block_header_t *next = block->free_list.next_free;
	ASAN_POISON_MEMORY_REGION(&block->free_list, sizeof(struct free_list));

	tlsf_assert(prev && "prev_free field can not be null");
	tlsf_assert(next && "next_free field can not be null");

	ASAN_UNPOISON_MEMORY_REGION(&next->free_list, sizeof(struct free_list));
	ASAN_UNPOISON_MEMORY_REGION(&prev->free_list, sizeof(struct free_list));

	next->free_list.prev_free = prev;
	prev->free_list.next_free = next;

	ASAN_POISON_MEMORY_REGION(&next->free_list, sizeof(struct free_list));
	ASAN_POISON_MEMORY_REGION(&prev->free_list, sizeof(struct free_list));

	/* If this block is the head of the free list, set new head */
	if (tlsf->blocks[fl][sl] == block) {
		tlsf->blocks[fl][sl] = next;

		/* If the new head is null, clear the bitmap */
		if (next == &tlsf->block_null) {
			tlsf->sl_bitmap[fl] &= ~(UINT64_C(1) << sl);

			/* If the second bitmap is now empty, clear the fl bitmap */
			if (tlsf->sl_bitmap[fl] == 0) {
				tlsf->fl_bitmap &= ~(UINT64_C(1) << fl);
			}
		}
	}
}

/* Insert a free block into the free block list */
// ASAN temporarily unpoisons free list of current free list head
static void insert_free_block(tlsf_t *tlsf, block_header_t *block, int fl, int sl)
{
	block_header_t *current = tlsf->blocks[fl][sl];
	tlsf_assert(current && "free list cannot have a null entry");
	tlsf_assert(block && "cannot insert a null entry into the free list");

	ASAN_UNPOISON_MEMORY_REGION(&current->free_list, sizeof(struct free_list));
	ASAN_UNPOISON_MEMORY_REGION(&block->free_list, sizeof(struct free_list));
	block->free_list.next_free = current;
	block->free_list.prev_free = &tlsf->block_null;
	current->free_list.prev_free = block;
	ASAN_POISON_MEMORY_REGION(&current->free_list, sizeof(struct free_list));
	ASAN_POISON_MEMORY_REGION(&block->free_list, sizeof(struct free_list));

	tlsf_assert(block_to_ptr(block) == align_ptr(block_to_ptr(block), ALIGN_SIZE)
		&& "block not aligned properly");
	/*
	 * Insert the new block at the head of the list, and mark the first-
	 * and second-level bitmaps appropriately.
	 */
	tlsf->blocks[fl][sl] = block;
	tlsf->fl_bitmap |= (UINT64_C(1) << fl);
	tlsf->sl_bitmap[fl] |= (UINT64_C(1) << sl);
}

/* Remove a given block from the free list */
// ASAN pre: unpoisoned metadata block
static void block_remove(tlsf_t *tlsf, block_header_t *block)
{
	int fl, sl;
	mapping_search(block_size(block), &fl, &sl);
	remove_free_block(tlsf, block, fl, sl);
}

/* Insert a given block into the free list */
// ASAN pre: unpoisoned metadata block
static void block_insert(tlsf_t *tlsf, block_header_t *block)
{
	int fl, sl;
	mapping_search(block_size(block), &fl, &sl);
	insert_free_block(tlsf, block, fl, sl);
}

// ASAN pre: unpoisoned metadata block
static int block_can_split(block_header_t *block, size_t size)
{
	return block_size(block) >= sizeof(block_header_t) + size;
}

/* Split a block into two, the second of which is free */
// ASAN pre: unpoisoned metadata block
// ASAN post: unpoisoned metadata block, unpoisoned result
static block_header_t *block_split(block_header_t *block, size_t new_size)
{
	const size_t old_size = block_size(block);

	block_set_size(block, new_size);

	/* Calculate the amount of space left in the remaining block */
	block_header_t *remaining = block_next(block);
	ASAN_UNPOISON_MEMORY_REGION(&remaining->metadata, sizeof(struct metadata));

	tlsf_assert(block_to_ptr(remaining) == align_ptr(block_to_ptr(remaining), ALIGN_SIZE)
		&& "remaining block not aligned properly");

	const size_t remain_size = old_size - (new_size + metadata_size);
	tlsf_assert(remain_size >= block_size_min && "block split with invalid size");
	block_set_size(remaining, remain_size);
	// Less frequent to set this here instead of in block_prepare_used()
	remaining->metadata.tlsf = block->metadata.tlsf;
	block_mark_as_free(remaining);


	return remaining;
}

/* Absorb a free block's storage into an adjacent previous free block */
// ASAN expect unpoisoned prev, block
// ASAN leaves them unpoisoned
static void block_absorb(block_header_t *prev, block_header_t *block)
{
	tlsf_assert(!block_is_last(prev) && "previous block can't be last");
	/* Note: Leaves flags untouched */
	prev->metadata.size += block_size(block) + metadata_size;
	block_link_next(prev);
}

/* Merge a just-freed block with an adjacent previous free block */
// ASAN pre expect unpoisoned header for block
// ASAN post return unpoisoned header for merged block
static void block_merge_prev(tlsf_t *tlsf, block_header_t **block)
{
	if (block_is_prev_free(*block)) {
		block_header_t *prev = block_prev(*block);
		ASAN_UNPOISON_MEMORY_REGION(&prev->metadata, sizeof(struct metadata));
		tlsf_assert(prev && "prev physical block can't be null");
		tlsf_assert(block_is_free(prev) && "prev block is not free though marked as such");
		block_remove(tlsf, prev);
		block_absorb(prev, *block);
		ASAN_POISON_MEMORY_REGION(&(*block)->metadata, sizeof(struct metadata));
		*block = prev;
	}
}

/* Merge a just-freed block with an adjacent free block */
// ASAN pre expect unpoisoned header for block
// ASAN post leave it unpoisoned
static void block_merge_next(tlsf_t *tlsf, block_header_t *block)
{
	block_header_t *next = block_next(block);
	tlsf_assert(next && "next physical block can't be null");
	ASAN_UNPOISON_MEMORY_REGION(&next->metadata, sizeof(struct metadata));

	if (block_is_free(next)) {
		tlsf_assert(!block_is_last(block) && "previous block can't be last");
		block_remove(tlsf, next);
		block_absorb(block, next);
	}

	// Next block's metadata becomes free memory
	ASAN_POISON_MEMORY_REGION(&next->metadata, sizeof(struct metadata));
}

/* Trim any trailing block space off the end of a block, return to pool */
// ASAN pre unpoisoned metadata block
// ASAN post unpoisoned metadata block
static void block_trim_free(tlsf_t *tlsf, block_header_t *block, size_t size)
{
	tlsf_assert(block_is_free(block) && "block must be free");
	if (block_can_split(block, size)) {
		block_header_t *remaining_block = block_split(block, size);
		block_link_next(block);
		block_set_prev_free(remaining_block);
		block_insert(tlsf, remaining_block);
		ASAN_POISON_MEMORY_REGION(&remaining_block->metadata, sizeof(struct metadata));
	}
}

/* Trim any trailing block space off the end of a used block, return to pool */
// ASAN pre unpoisoned metadata block
// ASAN post unpoisoned metadata block
static void block_trim_used(tlsf_t *tlsf, block_header_t *block, size_t size)
{
	tlsf_assert(!block_is_free(block) && "block must be used");
	if (block_can_split(block, size)) {
		/* If the next block is free, we must coalesce */
		block_header_t *remaining_block = block_split(block, size);
		block_set_prev_used(remaining_block);

		block_merge_next(tlsf, remaining_block);
		block_insert(tlsf, remaining_block);
		ASAN_POISON_MEMORY_REGION(&remaining_block->metadata, sizeof(struct metadata));
	}
}

/* If possible, create a trailing free block after trimming given block by size */
// ASAN pre unpoisoned metadata block
// ASAN post unpoisoned return, block should not be used
static void block_trim_free_leading(tlsf_t *tlsf, block_header_t **block, size_t size)
{
	block_header_t *remaining_block = *block;
	if (block_can_split(*block, size)) {
		/* We want the 2nd block */
		remaining_block = block_split(*block, size - metadata_size);
		block_set_prev_free(remaining_block);

		block_link_next(*block);
		block_insert(tlsf, *block);
		ASAN_POISON_MEMORY_REGION(&(*block)->metadata, sizeof(struct metadata));
		*block = remaining_block;
	}
}

// ASAN post: unpoisoned metadata block
static block_header_t *block_locate_free(tlsf_t *tlsf, size_t size)
{
	int fl = 0;
	int sl = 0;
	block_header_t *block = NULL;

	if (size > 0) {
		size_t adjusted = size;

		/* Round up to the next block size (for allocations) */
		if (size >= SMALL_BLOCK_SIZE) {
			const size_t round = (1 << (tlsf_fls_sizet(size) - SL_INDEX_COUNT_LOG2)) - 1;
			adjusted += round;
		}

		mapping_search(adjusted, &fl, &sl);

		/*
		 * The above can futz with the size, so for excessively large sizes it can sometimes wind up
		 * with indices that are off the end of the block array.
		 * So, we protect against that here.
		 * Note that we don't need to check sl, since it comes from a modulo operation that guarantees it's always in range.
		 */
		if (fl < FL_INDEX_COUNT) {
			block = search_suitable_block(tlsf, &fl, &sl);

			if (block != NULL) {
				tlsf_assert(block_size(block) >= size);
				remove_free_block(tlsf, block, fl, sl);
			}
		}
	}

	return block;
}

static void *block_prepare_used(tlsf_t *tlsf, block_header_t *block, size_t size)
{
	void *p = NULL;
	if (block != NULL) {
		tlsf_assert(size && "size must be non-zero");
		block_trim_free(tlsf, block, size);
		block_mark_as_used(block);
		assert(block->metadata.tlsf == tlsf);
#ifndef DNDEBUG
		block->metadata.allocation_addr = 0;
#endif
		p = block_to_ptr(block);
		ASAN_UNPOISON_MEMORY_REGION(p, block_size(block));
		ASAN_POISON_MEMORY_REGION(&block->metadata, sizeof(struct metadata));
	}
	return p;
}

/* Clear structure and point all empty lists at the null block */
static void control_construct(tlsf_t *tlsf)
{
	int i, j;

	tlsf->block_null.free_list.next_free = &tlsf->block_null;
	tlsf->block_null.free_list.prev_free = &tlsf->block_null;

	tlsf->fl_bitmap = 0;
	for (i = 0; i < FL_INDEX_COUNT; i++) {
		tlsf->sl_bitmap[i] = 0;
		for (j = 0; j < SL_INDEX_COUNT; j++) {
			tlsf->blocks[i][j] = &tlsf->block_null;
		}
	}

	ASAN_POISON_MEMORY_REGION(&tlsf->block_null.free_list, sizeof(struct free_list));
}

/*
 * Debugging utilities.
 */

typedef struct integrity {
	int prev_status;
	int status;
} integrity_t;

#define tlsf_insist(x) { tlsf_assert(x); if (!(x)) { status--; } }

static void integrity_walker(void *ptr, size_t size, int used, void *user)
{
	block_header_t *block = block_from_ptr(ptr);
	integrity_t *integ = tlsf_cast(integrity_t *, user);
	const int this_prev_status = block_is_prev_free(block) ? 1 : 0;
	const int this_status = block_is_free(block) ? 1 : 0;
	const size_t this_block_size = block_size(block);

	int status = 0;
	(void)used;
	tlsf_insist(integ->prev_status == this_prev_status && "prev status incorrect");
	tlsf_insist(size == this_block_size && "block size incorrect");

	integ->prev_status = this_status;
	integ->status += status;
}

int tlsf_check(tlsf_t *tlsf)
{
	int i, j;
	int status = 0;

	/* Check that the free lists and bitmaps are accurate */
	for (i = 0; i < FL_INDEX_COUNT; i++) {
		for (j = 0; j < SL_INDEX_COUNT; j++) {
			const uint64_t fl_map = tlsf->fl_bitmap & (UINT64_C(1) << i);
			const uint64_t sl_list = tlsf->sl_bitmap[i];
			const uint64_t sl_map = sl_list & (UINT64_C(1) << j);
			const block_header_t *block = tlsf->blocks[i][j];

			/* Check that first- and second-level lists agree */
			if (fl_map == 0) {
				tlsf_insist(!sl_map && "second-level map must be null");
			}

			if (sl_map == 0) {
				tlsf_insist(block == &tlsf->block_null && "block list must be null");
				continue;
			}

			/* Check that there is at least one free block */
			tlsf_insist(sl_list && "no free blocks in second-level map");
			tlsf_insist(block != &tlsf->block_null && "block should not be null");

			while (block != &tlsf->block_null) {
				int fli, sli;
				tlsf_insist(block_is_free(block) && "block should be free");
				tlsf_insist(!block_is_prev_free(block) && "blocks should have coalesced");
				tlsf_insist(!block_is_free(block_next(block)) && "blocks should have coalesced");
				tlsf_insist(block_is_prev_free(block_next(block)) && "block should be free");
				tlsf_insist(block_size(block) >= block_size_min && "block not minimum size");

				mapping_search(block_size(block), &fli, &sli);
				tlsf_insist(fli == i && sli == j && "block size indexed in wrong list");
				block = block->free_list.next_free;
			}
		}
	}

	return status;
}

#undef tlsf_insist

static void default_walker(void *ptr, size_t size, int used, void *user)
{
	(void)user;
	printf("\t%p %s size: %x (%p)\n", ptr, used ? "used" : "free", (unsigned int)size, block_from_ptr(ptr));
}

void tlsf_walk_pool(tlsf_pool_t *pool, tlsf_walker walker, void *user)
{
	tlsf_walker pool_walker = walker ? walker : default_walker;
	block_header_t *block = first_block(pool);

	while (block && !block_is_last(block)) {
		pool_walker(
			block_to_ptr(block),
			block_size(block),
			!block_is_free(block),
			user);
		block = block_next(block);
	}
}

size_t tlsf_block_size(void *ptr)
{
	size_t size = 0;
	if (ptr != NULL) {
		const block_header_t *block = block_from_ptr(ptr);

		ASAN_UNPOISON_MEMORY_REGION(&block->metadata, sizeof(struct metadata));
		size = block_size(block);
		ASAN_POISON_MEMORY_REGION(&block->metadata, sizeof(struct metadata));
		// needed because in realloc only the requested size is
		// unpoisoned, after this call, the whole block size is exposed
		ASAN_UNPOISON_MEMORY_REGION(ptr, size);
	}
	return size;
}

// Disable address sanitizer to allow retrieval of the heap without
// races with threads accessing adjacent allocations.
// This allows wrappers around TLSF to use per-heap locks:
// They find which lock to use by calling this function
// without the need to hold a global lock
ASAN_NO_SANITIZE_ADDRESS
tlsf_t *tlsf_from_ptr(void *ptr)
{
	const block_header_t *block = block_from_ptr(ptr);
	tlsf_t *tlsf;

	tlsf = block->metadata.tlsf;

	return tlsf;
}

int tlsf_check_pool(tlsf_pool_t *pool)
{
	/* Check that the blocks are physically correct */
	integrity_t integ = { 0, 0 };
	tlsf_walk_pool(pool, integrity_walker, &integ);

	return integ.status;
}

/*
 * Size of the TLSF structures in a given memory block passed to
 * tlsf_create, equal to the size of a tlsf_t
 */
size_t tlsf_size(void)
{
	return sizeof(tlsf_t);
}

size_t tlsf_align_size(void)
{
	return ALIGN_SIZE;
}

size_t tlsf_block_size_min(void)
{
	return block_size_min;
}

size_t tlsf_block_size_max(void)
{
	return block_size_max;
}

/*
 * Overhead of the TLSF structures in a given memory block passed to
 * tlsf_add_pool, equal to the overhead of a free block and the
 * sentinel block.
 */
size_t tlsf_pool_overhead(void)
{
	return 2 * metadata_size;
}

size_t tlsf_alloc_overhead(void)
{
	return metadata_size;
}

tlsf_pool_t *tlsf_add_pool(tlsf_t *tlsf, void *mem, size_t bytes)
{
	block_header_t *block;
	block_header_t *next;

	const size_t pool_overhead = tlsf_pool_overhead();
	const size_t pool_bytes = align_down(bytes - pool_overhead, ALIGN_SIZE);

	if (((ptrdiff_t)mem % ALIGN_SIZE) != 0) {
		printf("tlsf_add_pool: Memory must be aligned by %u bytes.\n",
			(unsigned int)ALIGN_SIZE);
		return NULL;
	}

	if (pool_bytes < block_size_min || pool_bytes > block_size_max) {
		printf("tlsf_add_pool: Memory size must be between %zu and %zu bytes.\n",
			pool_overhead + block_size_min,
			pool_overhead + block_size_max);
		return NULL;
	}

	/*
	 * Create the main free block. Offset the start of the block slightly
	 * so that the prev_phys_block field falls outside of the pool -
	 * it will never be used.
	 */
	block = first_block(mem);
	ASAN_UNPOISON_MEMORY_REGION(&block->metadata, sizeof(struct metadata));
	block_set_size(block, pool_bytes);
	block_set_free(block);
	block_set_prev_used(block);
	block_insert(tlsf, block);
	block->metadata.tlsf = tlsf;

	/* Split the block to create a zero-size sentinel block */
	next = block_next(block);
	ASAN_UNPOISON_MEMORY_REGION(&next->metadata, sizeof(struct metadata));
	block_link_next(block);
	block_set_size(next, 0);
	block_set_used(next);
	block_set_prev_free(next);
	next->metadata.tlsf = tlsf;

	ASAN_POISON_MEMORY_REGION(block_to_ptr(block), block_size(block));
	ASAN_POISON_MEMORY_REGION(&block->metadata, sizeof(struct metadata));
	ASAN_POISON_MEMORY_REGION(&next->metadata, sizeof(struct metadata));

	return mem;
}

void tlsf_remove_pool(tlsf_t *tlsf, tlsf_pool_t *pool)
{
	block_header_t *block = first_block(pool);
	ASAN_UNPOISON_MEMORY_REGION(&block->metadata, sizeof(struct metadata));
	block_header_t *next = block_next(block);
	ASAN_UNPOISON_MEMORY_REGION(&next->metadata, sizeof(struct metadata));

	int fl = 0;
	int sl = 0;

	tlsf_assert(block_is_free(block) && "block should be free");
	tlsf_assert(!block_is_free(next) && "next block should not be free");
	tlsf_assert(block_size(next) == 0 && "next block size should be zero");

	mapping_search(block_size(block), &fl, &sl);
	remove_free_block(tlsf, block, fl, sl);

	ASAN_UNPOISON_MEMORY_REGION(block_to_ptr(block), block_size(block));
}

/*
 * TLSF main interface.
 */

int test_ffs_fls()
{
	/* Verify ffs/fls work properly */
	int rv = 0;
	rv += (tlsf_ffsll(0) == -1) ? 0 : 0x1;
	rv += (tlsf_ffsll(1) == 0) ? 0 : 0x4;
	rv += (tlsf_ffsll(0x80000000) == 31) ? 0 : 0x10;
	rv += (tlsf_ffsll(0x80008000) == 15) ? 0 : 0x20;

	rv += (tlsf_fls_sizet(0x80000000) == 31) ? 0 : 0x100;
	rv += (tlsf_fls_sizet(0x100000000) == 32) ? 0 : 0x200;
	rv += (tlsf_fls_sizet(0xffffffffffffffff) == 63) ? 0 : 0x400;

	if (rv) {
		printf("test_ffs_fls: %x ffs/fls tests failed.\n", rv);
	}
	return rv;
}

tlsf_t *tlsf_create(void *mem)
{
	if (test_ffs_fls()) {
		printf("ffs/fls test failed\n");
		return NULL;
	}

	if (((ptrdiff_t)mem % ALIGN_SIZE) != 0) {
		printf("tlsf_create: Memory must be aligned to %u bytes.\n",
			(unsigned int)ALIGN_SIZE);
		return NULL;
	}

	control_construct(tlsf_cast(tlsf_t *, mem));

	return tlsf_cast(tlsf_t *, mem);
}

tlsf_t *tlsf_create_with_pool(void *mem, size_t bytes)
{
	tlsf_t *tlsf = tlsf_create(mem);
	tlsf_add_pool(tlsf, (char *)mem + tlsf_size(), bytes - tlsf_size());
	return tlsf;
}

void tlsf_destroy(tlsf_t *tlsf)
{
	ASAN_UNPOISON_MEMORY_REGION(&tlsf->block_null.free_list, sizeof(struct free_list));

	/* Nothing to do */
	(void)tlsf;
}

tlsf_pool_t *tlsf_get_pool(tlsf_t *tlsf)
{
	return tlsf_cast(tlsf_pool_t *, (char *)tlsf + tlsf_size());
}

#ifndef DNDEBUG
ASAN_NO_SANITIZE_ADDRESS void tlsf_track_allocation(void* addr, void* data)
{
	block_header_t *hdr = block_from_ptr(addr);
	hdr->metadata.allocation_addr = data;
}
#endif

void *tlsf_malloc(tlsf_t *tlsf, size_t size)
{
	return tlsf_memalign(tlsf, 16, size);
}

void *tlsf_memalign(tlsf_t *tlsf, size_t align, size_t size)
{
	const size_t adjust = adjust_request_size(size, ALIGN_SIZE);

	/*
	 * We must allocate an additional minimum block size bytes so that if
	 * our free block will leave an alignment gap which is smaller, we can
	 * trim a leading free block and release it back to the pool. We must
	 * do this because the previous physical block is in use, therefore
	 * the prev_phys_block field is not valid, and we can't simply adjust
	 * the size of that block.
	 */
	const size_t gap_minimum = sizeof(block_header_t);
	const size_t size_with_gap = adjust_request_size(adjust + align + gap_minimum, align);

	/*
	 * If alignment is less than or equals base alignment, we're done.
	 * If we requested 0 bytes, return null, as tlsf_malloc(0) does.
	 */
	const size_t aligned_size = (adjust && align > ALIGN_SIZE) ? size_with_gap : adjust;

	block_header_t *block = block_locate_free(tlsf, aligned_size);

	/* This can't be a static assert */
	tlsf_assert(sizeof(block_header_t) == block_size_min + metadata_size);

	if (block != NULL) {
		void *ptr = block_to_ptr(block);
		void *aligned = align_ptr(ptr, align);
		size_t gap = tlsf_cast(size_t,
			tlsf_cast(ptrdiff_t, aligned) - tlsf_cast(ptrdiff_t, ptr));

		/* If gap size is too small, offset to next aligned boundary */
		if (gap > 0 && gap < gap_minimum) {
			const size_t gap_remain = gap_minimum - gap;
			const size_t offset = tlsf_max(gap_remain, align);
			const void *next_aligned = tlsf_cast(void *,
				tlsf_cast(ptrdiff_t, aligned) + offset);

			aligned = align_ptr(next_aligned, align);
			gap = tlsf_cast(size_t,
				tlsf_cast(ptrdiff_t, aligned) - tlsf_cast(ptrdiff_t, ptr));
		}

		if (gap > 0) {
			tlsf_assert(gap >= gap_minimum && "gap size too small");
			block_trim_free_leading(tlsf, &block, gap);
		}
	}

	return block_prepare_used(tlsf, block, adjust);
}

void tlsf_free(tlsf_t *tlsf, void *ptr)
{
	/* Don't attempt to free a NULL pointer */
	if (ptr != NULL) {
		block_header_t *block = block_from_ptr(ptr);
		ASAN_UNPOISON_MEMORY_REGION(&block->metadata, sizeof(struct metadata));
		ASAN_POISON_MEMORY_REGION(ptr, block_size(block));
		tlsf_assert(!block_is_free(block) && "block already marked as free");
		block_mark_as_free(block);

		if (tlsf == NULL) {
			tlsf = block->metadata.tlsf;
		} else {
			assert(tlsf == block->metadata.tlsf && "invalid heap");
		}

		block_merge_prev(tlsf, &block);
		block_merge_next(tlsf, block);
		block_insert(tlsf, block);
		ASAN_POISON_MEMORY_REGION(&block->metadata, sizeof(struct metadata));
		// ASAN poison data block
	}
}

/*
 * The TLSF block information provides us with enough information to
 * provide a reasonably intelligent implementation of realloc, growing or
 * shrinking the currently allocated block as required.
 *
 * This routine handles the somewhat esoteric edge cases of realloc:
 * - a non-zero size with a null pointer will behave like malloc
 * - a zero size with a non-null pointer will behave like free
 * - a request that cannot be satisfied will leave the original buffer
 *   untouched
 * - an extended buffer size will leave the newly-allocated area with
 *   contents undefined
 */
void *tlsf_realloc(tlsf_t *tlsf, void *ptr, size_t size)
{
	void *p = NULL;

	/* Zero-size requests are treated as free */
	if (ptr != NULL && size == 0) {
		tlsf_free(tlsf, ptr);
	}
	/* Requests with NULL pointers are treated as malloc */
	else if (ptr == NULL) {
		tlsf_assert(tlsf != NULL && "realloc with NULL pointer requires heap argument");
		p = tlsf_malloc(tlsf, size);
	} else {
		block_header_t *block = block_from_ptr(ptr);
		ASAN_UNPOISON_MEMORY_REGION(&block->metadata, sizeof(struct metadata));
		block_header_t *next = block_next(block);
		ASAN_UNPOISON_MEMORY_REGION(&next->metadata, sizeof(struct metadata));

		const size_t cursize = block_size(block);
		const size_t combined = cursize + block_size(next) + metadata_size;
		const size_t adjust = adjust_request_size(size, ALIGN_SIZE);

		if (tlsf == NULL) {
			tlsf = block->metadata.tlsf;
		} else {
			tlsf_assert(tlsf == block->metadata.tlsf && "invalid heap");
		}

		tlsf_assert(!block_is_free(block) && "block already marked as free");

		/*
		 * If the next block is used, or when combined with the current
		 * block, does not offer enough space, we must reallocate and copy.
		 */
		if (adjust > cursize && (!block_is_free(next) || adjust > combined)) {

			ASAN_POISON_MEMORY_REGION(&block->metadata, sizeof(struct metadata));
			ASAN_POISON_MEMORY_REGION(&next->metadata, sizeof(struct metadata));
			p = tlsf_malloc(tlsf, size);
			if (p != NULL) {
				const size_t minsize = tlsf_min(cursize, size);
				// this needs to be an uninstrumented memcpy, so it doesn't trigger poisoning
#ifdef __KASAN__
				void* __memcpy(void* restrict dest, const void* restrict src, size_t n);
				__memcpy(p, ptr, minsize);
#else
				memcpy(p, ptr, minsize);
#endif
				tlsf_free(tlsf, ptr);
			}
		} else {
			ASAN_POISON_MEMORY_REGION(&next->metadata, sizeof(struct metadata));
			/* Do we need to expand to the next block? */
			if (adjust > cursize) {
				block_merge_next(tlsf, block);
				block_mark_as_used(block);

				// Unpoison extra
				ASAN_UNPOISON_MEMORY_REGION(ptr, adjust);
			} else if (adjust == cursize) {
				// Unpoison whole area
				ASAN_UNPOISON_MEMORY_REGION(ptr, adjust);
			} else {
				// Poison shrinked
				ASAN_POISON_MEMORY_REGION(ptr, cursize);
				ASAN_UNPOISON_MEMORY_REGION(ptr, adjust);
			}

			/* Trim the resulting block and return the original pointer */
			block_trim_used(tlsf, block, adjust);
			p = ptr;
			ASAN_POISON_MEMORY_REGION(&block->metadata, sizeof(struct metadata));
		}
	}
	return p;
}