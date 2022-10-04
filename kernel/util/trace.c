#include "trace.h"

#include <kernel.h>

#include <stddef.h>
#include <stdint.h>
#include "arch/intrin.h"
#include "debug/term.h"

void trace_init() {
#ifdef __GRAPHICS_TRACE__
    struct limine_framebuffer* fb = g_limine_framebuffer.response->framebuffers[0];
    term_init(fb->address, fb->width, fb->height, fb->pitch / 4);
    term_clear();
#endif

#ifdef __SERIAL_TRACE__
    __outbyte(0x3f8 + 1, 0x00);
    __outbyte(0x3f8 + 3, 0x80);
    __outbyte(0x3f8 + 0, 0x03);
    __outbyte(0x3f8 + 1, 0x00);
    __outbyte(0x3f8 + 3, 0x03);
    __outbyte(0x3f8 + 2, 0xC7);
    __outbyte(0x3f8 + 4, 0x0B);
    __outbyte(0x3f8 + 4, 0x0F);
#endif
}

void trace_hex(const void* _data, size_t size) {
    const uint8_t* data = _data;
    char ascii[17] = { 0 };

    printf("[*] ");
    for (int i = 0; i < size; i++) {
        printf("%02x ", data[i]);

        if (data[i] >= ' ' && data[i] <= '~') {
            ascii[i % 16] = data[i];
        } else {
            ascii[i % 16] = '.';
        }

        if ((i + 1) % 8 == 0 || i + 1 == size) {
            printf(" ");
            if ((i + 1) % 16 == 0) {
                printf("|  %s \n", ascii);
                if (i + 1 != size) {
                    printf("[*] ");
                }
            } else if (i + 1 == size) {
                ascii[(i + 1) % 16] = '\0';
                if ((i + 1) % 16 <= 8) {
                    printf(" ");
                }
                for (int j = (i + 1) % 16; j < 16; ++j) {
                    printf("   ");
                }
                printf("|  %s \n", ascii);
            }
        }
    }
}

void _putchar(char character) {
#ifdef __GRAPHICS_TRACE__
    term_print_char(character);
#endif
#ifdef __SERIAL_TRACE__
    while ((__inbyte(0x3f8 + 5) & 0x20) == 0);
    __outbyte(0x3f8, character);
#endif
}
