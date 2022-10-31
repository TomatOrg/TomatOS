#pragma once
#ifndef __KASAN__
#define ASAN_POISON_MEMORY_REGION(addr, size)
#define ASAN_UNPOISON_MEMORY_REGION(addr, size)
#define ASAN_NO_SANITIZE_ADDRESS
#else
#include <stdint.h>
#include <stddef.h>
#include <mem/mem.h>

#define KASAN_SANITIZED_START KERNEL_HEAP_START
#define KASAN_SANITIZED_END KERNEL_HEAP_END

/* Part of internal compiler interface */
#define KASAN_SHADOW_SCALE_SHIFT 3
#define KASAN_SHADOW_SCALE_SIZE (1 << KASAN_SHADOW_SCALE_SHIFT)

#define KASAN_CODE_STACK_LEFT 0xF1
#define KASAN_CODE_STACK_MID 0xF2
#define KASAN_CODE_STACK_RIGHT 0xF3

#define KASAN_CODE_FRESH_KVA 0xF9
#define KASAN_CODE_GLOBAL_OVERFLOW 0xFA
#define KASAN_CODE_KMEM_FREED 0xFB
#define KASAN_CODE_POOL_OVERFLOW 0xFC
#define KASAN_CODE_POOL_FREED 0xFD
#define KASAN_CODE_KMALLOC_OVERFLOW 0xFE
#define KASAN_CODE_KMALLOC_FREED 0xFF

#define KASAN_POOL_REDZONE_SIZE 8
#define KASAN_KMALLOC_REDZONE_SIZE 8


#define ASAN_POISON_MEMORY_REGION(addr, size) \
  kasan_poison_shadow((addr), (size), 0xFF)
#define ASAN_UNPOISON_MEMORY_REGION(addr, size) \
  kasan_unpoison_shadow((addr), (size))
#define ASAN_NO_SANITIZE_ADDRESS __attribute__((no_sanitize_address))

/* Initialize KASAN subsystem.
 *
 * Should be called during early kernel boot process, as soon as the shadow
 * memory is usable. */
err_t init_kasan(void);

/* Mark bytes as valid (in the shadow memory) */
void kasan_unpoison_shadow(const void *addr, size_t size);

/* Mark bytes as invalid (in the shadow memory) */
void kasan_poison_shadow(const void *addr, size_t size, uint8_t code);

/* Mark first 'size' bytes as valid (in the shadow memory), and the remaining
 * (size_with_redzone - size) bytes as invalid with given code. */
void kasan_mark(const void *addr, size_t size, size_t size_with_redzone,
                uint8_t code);
#endif