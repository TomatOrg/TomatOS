#include "log.h"

#include "arch/intrin.h"
#include "sync/spinlock.h"
#include "lib/defs.h"

#include <stdarg.h>

#include <flanterm.h>
#include <flanterm_backends/fb.h>

#include <limine.h>
#include <limine_requests.h>
#include <lib/printf.h>

#include "mem/alloc.h"
#include "mem/phys.h"


static irq_spinlock_t m_debug_lock = IRQ_SPINLOCK_INIT;

static struct flanterm_context* m_flanterm_context = NULL;

static bool m_e9_enabled = false;

void init_early_logging() {
    // detect e9 support
    m_e9_enabled = __inbyte(0xE9) == 0xE9;
}

static void phys_free_sized(void* ptr, size_t size) {
    phys_free(ptr);
}

void init_logging(void) {
    // framebuffer
    struct limine_framebuffer_response* response = g_limine_framebuffer_request.response;
    if (response != NULL && response->framebuffer_count >= 1) {
        struct limine_framebuffer* framebuffer = response->framebuffers[0];
        TRACE("Using framebuffer #0 - %p - %ldx%ld (pitch=%ld)", framebuffer->address, framebuffer->width, framebuffer->height, framebuffer->pitch);
        m_flanterm_context = flanterm_fb_init(
            phys_alloc,
            phys_free_sized,
            framebuffer->address, framebuffer->width, framebuffer->height, framebuffer->pitch,
            framebuffer->red_mask_size, framebuffer->red_mask_shift,
            framebuffer->green_mask_size, framebuffer->green_mask_shift,
            framebuffer->blue_mask_size, framebuffer->blue_mask_shift,
            NULL,
            NULL, NULL,
            NULL, NULL,
            NULL, NULL,
            NULL, 0, 0, 1,
            0, 0,
            0
        );
    }
}

void kputchar(char c) {
    if (m_flanterm_context != NULL) {
        flanterm_write(m_flanterm_context, &c, 1);
    }

    __outbyte(0xE9, c);
}

void debug_print(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    bool irq_state = irq_spinlock_acquire(&m_debug_lock);
    kvprintf(fmt, args);
    irq_spinlock_release(&m_debug_lock, irq_state);
    va_end(args);
}

void debug_vprint(const char* prefix, const char* suffix, const char* fmt, va_list va) {
    bool irq_state = irq_spinlock_acquire(&m_debug_lock);
    kprintf("%s", prefix);
    kvprintf(fmt, va);
    kprintf("%s", suffix);
    irq_spinlock_release(&m_debug_lock, irq_state);
}
