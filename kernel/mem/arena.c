#include "arena.h"

#include <util/string.h>
#include <mem/malloc.h>
#include <util/defs.h>

#include <stddef.h>
#include <stdint.h>
#include <limits.h>

/**
 * Code is based on https://www.25thandclement.com/~william/projects/libarena.html
 *
 * License (With modifications):
 *
 * Copyright (c) 2006  William Ahern
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
 * NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Macros to help with alignment
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * Taken from glibc 2.3.5 ptmalloc2 implementation. Seems reasonable.
 * Possible alternative: (sizeof (union { void *p; double d; })).
 */
#ifndef ARENA_SYSTEM_ALIGNMENT
#define ARENA_SYSTEM_ALIGNMENT	(2 * sizeof (size_t))
#endif

/*
 * Calculates the adjustment needed to push `p' to boundary `align'.
 *
 * NOTE: `align' MUST BE a power of 2.
 */
#ifndef ARENA_BOUNDARY_OFFSETOF
#define ARENA_BOUNDARY_OFFSETOF(p,align) \
	(((align) - ((uintptr_t)(p) % (align))) & ~(align))
#endif


/*
 * Calculates the adjustment needed to push `p' to boundary `align'.  This
 * version does not constrain the arguments, specifically it does not
 * require `align' to be a power of 2.
 */
#ifndef ARENA_XBOUNDARY_OFFSETOF
#define ARENA_XBOUNDARY_OFFSETOF(p,align) \
	((((align) - ((uintptr_t)(p) % (align))) != (align))? ((align) - ((uintptr_t)(p) % (align))) : (align))
#endif


/*
 * Determine a safe, but not necessarily smallest, boundary that type `t'
 * can be aligned on.
 */
#ifndef arena_alignof
#define arena_alignof(t)	offsetof(struct { char c; t x; }, x)
#endif


/*
 * Is `i' a power of 2?
 */
#ifndef arena_powerof2
#define arena_powerof2(i)	((((i) - 1) & (i)) == 0)
#endif


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Reverse Variable-length Bit-strings
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef rbitsint_t
#define rbitsint_t	size_t
#endif


/* Maximum space needed to store an rbitsint_t using CHAR_BIT - 1 bits */
#define RBITS_MAXLEN	(sizeof (rbitsint_t) + (((sizeof (rbitsint_t) * CHAR_BIT) - (sizeof (rbitsint_t) * (CHAR_BIT - 1))) / CHAR_BIT) + 1)


/*
 * Store the bit value representation of an rbitsint_t integer across buf,
 * starting from the end, preserving the highest bit of each unsigned char
 * in buf for use as a delimiter. Returns the last part of buf (lowest
 * unsigned char *) that was written to. If compact is set this will be the
 * part that holds the highest order bit of size (equal or greater than
 * buf), otherwise buf.
 */
static inline unsigned char *rbits_put(unsigned char *buf, size_t buflen, rbitsint_t i, int compact) {
    unsigned char *c	= &buf[buflen];
    unsigned char *last	= c;

    /* Iterate backwards, storing the size in all but the highest bit of
     * each byte. The highest bit serves as a marker telling us when to
     * stop.
     */
    do {
        c--;

        /* Assign all but the highest bit, which is preserved. */
        if ((*c = i & ~(1U << (CHAR_BIT - 1))))
            last	= c;

        i >>= CHAR_BIT - 1;
    } while (c > buf);

    if (!compact)
        last	= buf;

    /* Tag our terminal byte */
    *last	|= 1U << (CHAR_BIT - 1);

    return last;
} /* rbits_put() */


/*
 * Return the buffer size required to hold an rbitsint_t value bit-string
 * representation.
 */
static inline size_t rbits_len(rbitsint_t i) {
    unsigned char buf[RBITS_MAXLEN];
    unsigned char *pos	= rbits_put(buf,sizeof buf,i,1);

    return &buf[sizeof buf] - pos;
} /* rbits_len() */


/*
 * Return the offset from p required to 1) store the requested size and 2)
 * align to the desired alignment.
 */
static inline size_t rbits_ptroffset(unsigned char *p, size_t size, size_t align) {
    unsigned char lenbuf[RBITS_MAXLEN];
    unsigned char *lenend	= &lenbuf[sizeof lenbuf - 1];
    unsigned char *lenpos;
    uintptr_t ptrpos	= (uintptr_t)p;

    lenpos	= rbits_put(lenbuf,sizeof lenbuf,size,1);

    ptrpos	+= (lenend - lenpos) + 1;
    ptrpos	+= ARENA_BOUNDARY_OFFSETOF(ptrpos,align); /* Needs power of 2. */

    return ptrpos - (uintptr_t)p;
} /* rbits_ptroffset() */


/*
 * Beginning from *p, work backwards reconstructing the value of an
 * rbitsint_t integer. Stop when the highest order bit of *p is set, which
 * should have been previously preserved as a marker. Return the
 * reconstructed value, setting *end to the last position used of p.
 */
static inline rbitsint_t rbits_get(unsigned char *p, unsigned char **end) {
    rbitsint_t i	= 0;
    int n		= 0;

    do {
        i	|= (*p & ~(1 << (CHAR_BIT - 1))) << (n++ * (CHAR_BIT - 1));
    } while (!(*(p--) & (1 << (CHAR_BIT - 1))));

    *end	= p + 1;

    return i;
} /* rbits_get() */

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TODO: rewrite to use our own lists to save on space and have more consistency

/*	$OpenBSD: queue.h,v 1.27 2005/02/25 13:29:30 deraadt Exp $	*/
/*	$NetBSD: queue.h,v 1.11 1996/05/16 05:17:14 mycroft Exp $	*/

/*
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)queue.h	8.5 (Berkeley) 8/20/94
 */

/*
 * Singly-linked List definitions.
 */
#define SLIST_HEAD(name, type)						\
struct name {								\
	struct type *slh_first;	/* first element */			\
}

#define	SLIST_HEAD_INITIALIZER(head)					\
	{ NULL }

#define SLIST_ENTRY(type)						\
struct {								\
	struct type *sle_next;	/* next element */			\
}

/*
 * Singly-linked List access methods.
 */
#define	SLIST_FIRST(head)	((head)->slh_first)
#define	SLIST_END(head)		NULL
#define	SLIST_EMPTY(head)	(SLIST_FIRST(head) == SLIST_END(head))
#define	SLIST_NEXT(elm, field)	((elm)->field.sle_next)

#define	SLIST_FOREACH(var, head, field)					\
	for((var) = SLIST_FIRST(head);					\
	    (var) != SLIST_END(head);					\
	    (var) = SLIST_NEXT(var, field))

#define	SLIST_FOREACH_PREVPTR(var, varp, head, field)			\
	for ((varp) = &SLIST_FIRST((head));				\
	    ((var) = *(varp)) != SLIST_END(head);			\
	    (varp) = &SLIST_NEXT((var), field))

/*
 * Singly-linked List functions.
 */
#define	SLIST_INIT(head) {						\
	SLIST_FIRST(head) = SLIST_END(head);				\
}

#define	SLIST_INSERT_AFTER(slistelm, elm, field) do {			\
	(elm)->field.sle_next = (slistelm)->field.sle_next;		\
	(slistelm)->field.sle_next = (elm);				\
} while (0)

#define	SLIST_INSERT_HEAD(head, elm, field) do {			\
	(elm)->field.sle_next = (head)->slh_first;			\
	(head)->slh_first = (elm);					\
} while (0)

#define	SLIST_REMOVE_NEXT(head, elm, field) do {			\
	(elm)->field.sle_next = (elm)->field.sle_next->field.sle_next;	\
} while (0)

#define	SLIST_REMOVE_HEAD(head, field) do {				\
	(head)->slh_first = (head)->slh_first->field.sle_next;		\
} while (0)

#define SLIST_REMOVE(head, elm, type, field) do {			\
	if ((head)->slh_first == (elm)) {				\
		SLIST_REMOVE_HEAD((head), field);			\
	} else {							\
		struct type *curelm = (head)->slh_first;		\
									\
		while (curelm->field.sle_next != (elm))			\
			curelm = curelm->field.sle_next;		\
		curelm->field.sle_next =				\
		    curelm->field.sle_next->field.sle_next;		\
	}								\
} while (0)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// The arena allocator itself
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define roundup(x, y)	((((x) + ((y) - 1)) / (y)) * (y))

typedef struct arena_block {
    size_t size;

    struct {
        uint8_t* next;
    } pos;

    SLIST_ENTRY(arena_block) sle;

    uint8_t bytes[];
} arena_block_t;

#define ARENA_DEFAULT_ALIGNMENT ARENA_SYSTEM_ALIGNMENT

#define ARENA_DEFAULT_BLOCKLEN (1 << 15)

struct arena {
    SLIST_HEAD(,arena_block) blocks;
    unsigned int nblocks;
};

static arena_block_t* arena_block_alloc(size_t len) {
    size_t size = MAX(ARENA_DEFAULT_BLOCKLEN, offsetof(arena_block_t, bytes) + len + ARENA_DEFAULT_ALIGNMENT - 1 + RBITS_MAXLEN);
    arena_block_t* block = malloc(size);
    if (block == NULL) return NULL;
    block->size = size - offsetof(arena_block_t, bytes);
    block->pos.next = block->bytes;
    return block;
}

arena_t* create_arena() {
    // allocate the initial block
    arena_block_t* block = arena_block_alloc(sizeof(arena_t));
    if (block == NULL) return NULL;

    // allocate the arena from the block
    arena_t* arena = (arena_t*) (block->pos.next + rbits_ptroffset(block->pos.next, sizeof(arena_t), ARENA_SYSTEM_ALIGNMENT));
    rbits_put(block->pos.next, (uint8_t*)arena - block->pos.next, sizeof(arena_t), 0);
    block->pos.next = (void*)(arena + 1);

    // initialize the arena and insert the rest of the block
    SLIST_INIT(&arena->blocks);
    SLIST_INSERT_HEAD(&arena->blocks, block, sle);
    arena->nblocks = 1;

    return arena;
}

void free_arena(arena_t* arena) {
    if (arena != NULL) {
        arena_block_t* next = NULL;
        for (arena_block_t* block = SLIST_FIRST(&arena->blocks); block != NULL; block = next) {
            next = SLIST_NEXT(block, sle);
            free(block);
        }
    }
}

void* arena_alloc(arena_t* arena, size_t size) {
    if (size == 0) {
        return NULL;
    }

    // try to get a block from the head
    arena_block_t* block = SLIST_FIRST(&arena->blocks);
    uint8_t* ptr = block->pos.next + rbits_ptroffset(block->pos.next, size, ARENA_DEFAULT_ALIGNMENT);

    // no space for allocating from the head
    if (!(ptr + size <= &block->bytes[block->size])) {

        // calculate how much we want
        size_t want = 0;
        if (size > ARENA_DEFAULT_BLOCKLEN) {
            want = roundup(2 * size, MAX(1, ARENA_DEFAULT_BLOCKLEN));
        } else {
            want = size;
        }

        // allocate the new block
        block = arena_block_alloc(want);
        if (block == NULL) return NULL;

        // insert the block
        SLIST_INSERT_HEAD(&arena->blocks , block, sle);
        arena->nblocks++;

        ptr = block->pos.next + rbits_ptroffset(block->pos.next,size,ARENA_DEFAULT_ALIGNMENT);
    }

    // insert that we allocated that
    rbits_put(block->pos.next,ptr - block->pos.next,size,0);
    block->pos.next	= ptr + size;

    memset(ptr, 0, size);
    return ptr;
}
