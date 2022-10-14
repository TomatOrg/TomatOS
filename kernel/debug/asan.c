// taken from https://github.com/cahirwpz/mimiker

#ifdef KASAN
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <arch/intrin.h>
#include <libc/assert.h>
#include <mem/mem.h>
#include <arch/idt.h>
#include <mem/vmm.h>
#include "asan.h"

#define INLINE __attribute__((always_inline)) static inline
#define KASAN_ALLOCA_REDZONE_SIZE 32
#define KASAN_SHADOW_MASK (KASAN_SHADOW_SCALE_SIZE - 1)
#define KASAN_OFFSET 0xDFFFE00000000000ul
#define __predict_false(exp) __builtin_expect((exp) != 0, 0)

void *__memcpy(void *dst, const void *src, size_t n);
void *__memmove(void *dst, const void *src, size_t n);
void *__memset(void *dst, int c, size_t n);
int m_kasan_ready = 0;

INLINE int8_t *addr_to_shad(uintptr_t addr) { return (int8_t *)(KASAN_OFFSET + (addr >> KASAN_SHADOW_SCALE_SHIFT)); }
INLINE bool addr_supported(uintptr_t addr) { return addr >= KASAN_SANITIZED_START && addr < KASAN_SANITIZED_END; }
INLINE uint8_t shadow_1byte_isvalid(uintptr_t addr) {
	int8_t shadow_val = *addr_to_shad(addr);
	int8_t last = addr & KASAN_SHADOW_MASK;
	return (shadow_val == 0 || last < shadow_val) ? 0 : shadow_val;
}
INLINE uint8_t shadow_2byte_isvalid(uintptr_t addr) {
	int8_t shadow_val = *addr_to_shad(addr);
	int8_t last = (addr + 1) & KASAN_SHADOW_MASK;
	return (shadow_val == 0 || last < shadow_val) ? 0 : shadow_val;
}
INLINE uint8_t shadow_4byte_isvalid(uintptr_t addr) {
	int8_t shadow_val = *addr_to_shad(addr);
	int8_t last = (addr + 3) & KASAN_SHADOW_MASK;
	return (shadow_val == 0 || last < shadow_val) ? 0 : shadow_val;
}

INLINE uint8_t shadow_8byte_isvalid(uintptr_t addr) {
	int8_t shadow_val = *addr_to_shad(addr);
	int8_t last = (addr + 7) & KASAN_SHADOW_MASK;
	return (shadow_val == 0 || last < shadow_val) ? 0 : shadow_val;
}

INLINE uint8_t shadow_Nbyte_isvalid(uintptr_t addr, size_t size) {
	if (__builtin_constant_p(size)) {
		if (size == 1 || size == 2 || size == 4 || size == 8) {
			int8_t shadow_val = *addr_to_shad(addr);
			int8_t last = (addr + size - 1) & KASAN_SHADOW_MASK;
			return (shadow_val == 0 || last < shadow_val) ? 0 : shadow_val;
		}
	}

	while (size && (addr & KASAN_SHADOW_MASK)) {
		int8_t shadow_val = shadow_1byte_isvalid(addr);
		if (__predict_false(shadow_val))
			return shadow_val;
		addr++, size--;
	}

	while (size >= KASAN_SHADOW_SCALE_SIZE) {
		int8_t shadow_val = *addr_to_shad(addr);
		if (__predict_false(shadow_val))
			return shadow_val;
		addr += KASAN_SHADOW_SCALE_SIZE, size -= KASAN_SHADOW_SCALE_SIZE;
	}

	while (size) {
		int8_t shadow_val = shadow_1byte_isvalid(addr);
		if (__predict_false(shadow_val))
			return shadow_val;
		addr++, size--;
	}

	return 0;
}

static void kasan_panic(uintptr_t addr, size_t size, bool read, uint8_t code, void *ip, void *bp) {
	ERROR("kasan: %s at %p (size %lu) from %p", (read ? "read" : "write"), addr, size, ip);
	for (int i = -8; i < 8; i++) {
		uint8_t shadow_val = *addr_to_shad(addr + i * 8);
		if (i == 0) printf("<%02x> ", shadow_val);
		else printf("%02x ", shadow_val);
	}
	printf("\n");
}

INLINE uint8_t shadow_isvalid(uintptr_t addr, size_t size) {
	if (__builtin_constant_p(size)) {
		if (size == 1) return shadow_1byte_isvalid(addr);
		if (size == 2) return shadow_2byte_isvalid(addr);
		if (size == 4) return shadow_4byte_isvalid(addr);
		if (size == 8) return shadow_8byte_isvalid(addr);
	}
	return shadow_Nbyte_isvalid(addr, size);
}

INLINE void shadow_check(uintptr_t addr, size_t size, bool read, void *ip, void *bp) {
	if (__predict_false(!m_kasan_ready)) return;
	if (__predict_false(!addr_supported(addr))) return;
	uint8_t code = shadow_isvalid(addr, size);
	if (__predict_false(code)) kasan_panic(addr, size, read, code, ip, bp);
}

void kasan_mark(const void *addr, size_t valid, size_t total, uint8_t code) {
	int8_t *shadow = addr_to_shad((uintptr_t)addr);
	int8_t *end = shadow + total / KASAN_SHADOW_SCALE_SIZE;

	/* Valid bytes. */
	size_t len = valid / KASAN_SHADOW_SCALE_SIZE;
	__memset(shadow, 0, len);
	shadow += len;

	/* At most one partially valid byte. */
	if (valid & KASAN_SHADOW_MASK)
		*shadow++ = valid & KASAN_SHADOW_MASK;

	/* Invalid bytes. */
	if (shadow < end)
		__memset(shadow, code, end - shadow);
}

void kasan_unpoison_shadow(const void *addr, size_t size) {
	if (!addr_supported((uintptr_t)addr)) return;
	kasan_mark(addr,  (size + 7) & (~7),  (size + 7) & (~7), 0);
}

void kasan_poison_shadow(const void *addr, size_t size, uint8_t code) {
	if (!addr_supported((uintptr_t)addr)) return;
	kasan_mark(addr, 0, (size + 7) & (~7), code);
}

err_t init_kasan(void) {
	err_t err;
	uintptr_t start_off = KASAN_OFFSET + KERNEL_HEAP_START / 8;
	uintptr_t end_off = KASAN_OFFSET + KERNEL_HEAP_END / 8;
	CHECK_AND_RETHROW(vmm_alloc((void*)start_off, (end_off - start_off) / 4096, MAP_WRITE));
	kasan_poison_shadow((const void *)KASAN_SANITIZED_START, KASAN_SANITIZED_END - KASAN_SANITIZED_START, KASAN_CODE_FRESH_KVA);
	m_kasan_ready = 1;
cleanup:
  return err;
}

#define DEFINE_ASAN_LOAD_STORE(size) \
	void __asan_load##size##_noabort(uintptr_t addr) { \
		shadow_check(addr, size, true, __builtin_return_address(0), __builtin_frame_address(0)); \
	} \
	void __asan_report_load##size##_noabort(uintptr_t addr) { \
		shadow_check(addr, size, true, __builtin_return_address(0), __builtin_frame_address(0)); \
	} \
	void __asan_store##size##_noabort(uintptr_t addr) { \
		shadow_check(addr, size, false, __builtin_return_address(0), __builtin_frame_address(0)); \
	} \
	void __asan_report_store##size##_noabort(uintptr_t addr) { \
		shadow_check(addr, size, false, __builtin_return_address(0), __builtin_frame_address(0)); \
	}

DEFINE_ASAN_LOAD_STORE(1);
DEFINE_ASAN_LOAD_STORE(2);
DEFINE_ASAN_LOAD_STORE(4);
DEFINE_ASAN_LOAD_STORE(8);
DEFINE_ASAN_LOAD_STORE(16);

void __asan_loadN_noabort(uintptr_t addr, size_t size) {
	shadow_check(addr, size, true, __builtin_return_address(0),  __builtin_frame_address(0));
}
void __asan_report_load_n_noabort(uintptr_t addr, size_t size) {
	shadow_check(addr, size, true, __builtin_return_address(0),  __builtin_frame_address(0));
}

void __asan_storeN_noabort(uintptr_t addr, size_t size) {
	shadow_check(addr, size, false, __builtin_return_address(0),  __builtin_frame_address(0));
}
void __asan_report_store_n_noabort(uintptr_t addr, size_t size) {
	shadow_check(addr, size, false, __builtin_return_address(0),  __builtin_frame_address(0));
}

void __asan_handle_no_return(void) { }

void *memcpy(void *dst, const void *src, size_t n) {
	__asan_loadN_noabort((uintptr_t)src, n);
	__asan_storeN_noabort((uintptr_t)dst, n);
	return __memcpy(dst, src, n);
}

void *memmove(void *dst, const void *src, size_t n) {
	__asan_loadN_noabort((uintptr_t)src, n);
	__asan_storeN_noabort((uintptr_t)dst, n);
	return __memmove(dst, src, n);
}

void *memset(void *dst, int c, size_t n) {
	__asan_storeN_noabort((uintptr_t)dst, n);
	return __memset(dst, c, n);
}
#endif