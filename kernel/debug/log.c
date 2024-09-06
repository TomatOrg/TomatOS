#include "log.h"

#include "arch/intrin.h"
#include "sync/spinlock.h"
#include "lib/stb_sprintf.h"
#include "lib/defs.h"

#include <stdarg.h>

#include <backends/fb.h>
#include <flanterm.h>

#include <limine.h>

/**
 * The framebuffer request
 */
LIMINE_REQUEST struct limine_framebuffer_request g_limine_framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
};

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

static char* debug_callback(const char* buf, void* user, int len) {
    if (m_flanterm_context != NULL) {
        flanterm_write(m_flanterm_context, buf, len);
    }

    while (*buf && len--) {
        __outbyte(0xE9, *buf);
        buf++;
    }

    return user;
}

static spinlock_t m_debug_lock = INIT_SPINLOCK();

void debug_printf(const char* fmt, ...) {
    char buffer[STB_SPRINTF_MIN];

    spinlock_lock(&m_debug_lock);

    va_list args;
    va_start(args, fmt);
    stbsp_vsprintfcb(debug_callback, buffer, buffer, fmt, args);
    va_end(args);

    spinlock_unlock(&m_debug_lock);
}

void debug_vprintf(const char* fmt, va_list ap) {
    char buffer[STB_SPRINTF_MIN];

    spinlock_lock(&m_debug_lock);
    stbsp_vsprintfcb(debug_callback, buffer, buffer, fmt, ap);
    spinlock_unlock(&m_debug_lock);
}

void log_vprintf(const char* prefix, const char* fmt, va_list ap) {
    char buffer[STB_SPRINTF_MIN];

    spinlock_lock(&m_debug_lock);

    debug_callback(prefix, NULL, __builtin_strlen(prefix));
    stbsp_vsprintfcb(debug_callback, buffer, buffer, fmt, ap);
    debug_callback("\n", NULL, 1);

    spinlock_unlock(&m_debug_lock);
}
