#include "log.h"

#include "arch/intrin.h"
#include "sync/spinlock.h"
#include "lib/defs.h"

#include <stdarg.h>

#include <backends/fb.h>
#include <flanterm.h>

#include <limine.h>
#include <limine_requests.h>
#include <lib/printf.h>

/**
 * The flanterm context used for early debugging
 */
static struct flanterm_context* m_flanterm_context = NULL;

void init_early_logging() {
    // take the first framebuffer we find and use it for debugging
    // TODO: use multiple if possible?
    if (
        g_limine_framebuffer_request.response != NULL &&
        g_limine_framebuffer_request.response->framebuffer_count >= 1
    ) {
        struct limine_framebuffer* framebuffer = g_limine_framebuffer_request.response->framebuffers[0];
        LOG_DEBUG("Using framebuffer #0 - %p - %dx%d (pitch=%d)", framebuffer->address, framebuffer->width, framebuffer->height, framebuffer->pitch);
        m_flanterm_context = flanterm_fb_simple_init(framebuffer->address, framebuffer->width, framebuffer->height, framebuffer->pitch);
    }
}

static void kputchar(char c) {
    if (m_flanterm_context != NULL) {
        flanterm_write(m_flanterm_context, &c, 1);
    }

    __outbyte(0xE9, c);
}

static irq_spinlock_t m_debug_lock = INIT_IRQ_SPINLOCK();

void debug_printf(const char* fmt, ...) {
    bool irq_state = irq_spinlock_lock(&m_debug_lock);

    va_list args;
    va_start(args, fmt);
    kvprintf(fmt, args);
    va_end(args);

    irq_spinlock_unlock(&m_debug_lock, irq_state);
}

void debug_vprintf(const char* fmt, va_list ap) {
    bool irq_state = irq_spinlock_lock(&m_debug_lock);
    kvprintf(fmt, ap);
    irq_spinlock_unlock(&m_debug_lock, irq_state);
}

void log_vprintf(const char* prefix, const char* fmt, va_list ap) {
    bool irq_state = irq_spinlock_lock(&m_debug_lock);

    kprintf("%s", prefix);
    kvprintf(fmt, ap);
    kputchar('\n');

    irq_spinlock_unlock(&m_debug_lock, irq_state);
}

void log_vprintf_nonewline(const char* prefix, const char* fmt, va_list ap) {
    bool irq_state = irq_spinlock_lock(&m_debug_lock);

    kprintf("%s", prefix);
    kvprintf(fmt, ap);

    irq_spinlock_unlock(&m_debug_lock, irq_state);
}
