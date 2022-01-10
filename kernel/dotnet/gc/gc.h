#pragma once

#include <dotnet/type.h>

/**
 * This code is based on https://github.com/GregorR/gc
 *
 * License (with modifications):
 * Copyright (c) 2014, 2015 Gregor Richards
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * This is log2, so it will actually be 16MB
 */
#define GC_POOL_SIZE 24

/**
 * This is log2, so it will actually be 4KB
 */
#define GC_CARD_SIZE 12

#define GC_WORD_SIZEOF(x)   ((sizeof(x) + sizeof(size_t) - 1) / sizeof(size_t))
#define GC_POOL_BYTES       ((size_t) 1 << GC_POOL_SIZE)
#define GC_POOL_OUTER_MASK  ((size_t) -1 << GC_POOL_SIZE)
#define GC_POOL_INNER_MASK  (~GC_POOL_OUTER_MASK)
#define GC_POOL_OF(ptr)     ((gc_pool_t*) ((size_t) (ptr) & GC_POOL_OUTER_MASK))
#define GC_GEN_OF(ptr)      (GC_POOL_OF(ptr)->gen)
#define GC_CARD_BYTES       ((size_t) 1 << GC_CARD_SIZE)
#define GC_CARD_OUTER_MASK  ((size_t) -1 << GC_CARD_SIZE)
#define GC_CARD_INNER_MASK  (~GC_CARD_OUTER_MASK)
#define GC_CARDS_PER_POOL   ((size_t) 1 << (GC_POOL_SIZE-GC_CARD_SIZE))
#define GC_CARD_OF(ptr)     (((size_t) (ptr) & GC_POOL_INNER_MASK) >> GC_CARD_SIZE)
#define GC_BITS_PER_WORD    (8 * sizeof(size_t))
#define GC_WORDS_PER_POOL   (GC_POOL_BYTES / sizeof(size_t))

typedef struct gc_pool {
    // the remembered set for this pool.
    // NOTE: It's important this be first to make assigning
    //       to the remembered set take one less operation
    uint8_t remember[GC_CARDS_PER_POOL];

    // the locations of objects within the cards
    uint16_t first_object[GC_CARDS_PER_POOL];

    // the generation of this pool
    uint8_t gen;

    // size of the break table (in entries, used only during collection)
    size_t break_table_size;

    // pointer to the break table (used only during collection)
    void* break_table;

    // the next pool in this generation
    struct gc_pool* next;

    // the current free space and end of the pool
    size_t* free;
    size_t* end;

    // how much survived the last collection
    size_t survivors;

    // and the actual content
    size_t start[1];
} gc_pool_t;

#define GC_WB(object, member, value) \
    ({ \
        gc_pool_t* __pool = GC_POOL_OF(object); \
        if (__pool->gen) { \
            __pool->remember[GC_CARD_OF(object)] = 1; \
        } \
        (object)->member = value; \
    })

void* gc_alloc(type_t type);

void* gc_alloc_array(type_t type, size_t size);
