#include "term.h"

#include <string.h>
#include <stdint.h>

#define FONT_WIDTH 8
#define FONT_HEIGHT 16

static uint32_t* m_framebuffer;
static size_t m_framebuffer_pitch;
static size_t m_framebuffer_height;

static size_t m_term_cursor_x = 0;
static size_t m_term_cursor_y = 0;
static size_t m_term_height;
static size_t m_term_width;

const uint32_t font_width = 8, font_height = 16;

void term_init(uint32_t* framebuffer, size_t width, size_t height, size_t pitch) {
    m_framebuffer = framebuffer;
    m_framebuffer_height = height;
    m_framebuffer_pitch = pitch;

    m_term_height = height / FONT_HEIGHT;
    m_term_width = width / FONT_WIDTH;
}

extern uint8_t font_data[];

static void term_print_char_at(unsigned char c, int x, int y) {
	size_t font_off = (size_t)c * ((font_height * font_width) / 8);
	size_t fb_off = x + y * m_framebuffer_pitch;

	for (size_t yy = 0; yy < font_height; yy++) {
		uint8_t byte = font_data[font_off + yy];
		size_t tmp_fb_off = fb_off;
		for (size_t xx = 0; xx < font_width; xx++) {
			uint8_t mask = 1 << (7 - xx);
			uint32_t col = (byte & mask) ? 0xFFFFFFFF : 0x00000000;
			m_framebuffer[tmp_fb_off] = col;
			tmp_fb_off++;
		}
		fb_off += m_framebuffer_pitch;
	}
}

static void term_scoll_up(void) {
	memcpy(m_framebuffer, (void *)((uintptr_t)m_framebuffer + m_framebuffer_pitch * font_height * 4), m_framebuffer_pitch * (m_framebuffer_height - font_height) * 4);
	memset((void *)((uintptr_t)m_framebuffer + (m_framebuffer_height - font_height) * m_framebuffer_pitch * 4), 0, m_framebuffer_pitch * font_height * 4);
}

void term_print_char(char c) {
    if (m_framebuffer == NULL) {
        return;
    }

    // ignore
    if (c == '\r') {
        return;
    }

    // new line
	if (c == '\n') {
        m_term_cursor_x = 0;
		m_term_cursor_y++;
		if (m_term_cursor_y >= m_term_height) {
            term_scoll_up();
            m_term_cursor_y = m_term_height - 1;
		}
		return;
	}

	if (c == '\t') {
        m_term_cursor_x += 8;
		goto scroll_screen;
	}

    term_print_char_at(c, m_term_cursor_x * font_width, m_term_cursor_y * font_height);
	m_term_cursor_x++;

scroll_screen:
	if (m_term_cursor_x >= m_term_width) {
        m_term_cursor_x = 0;
		m_term_cursor_y++;
		if (m_term_cursor_y >= m_term_height) {
            term_scoll_up();
            m_term_cursor_y = m_term_height - 1;
		}
	}
}

void term_clear() {
    m_term_cursor_x = 0;
    m_term_cursor_y = 0;
    for (size_t i = 0; i < m_framebuffer_pitch * m_framebuffer_height; i++) {
        m_framebuffer[i] = 0;
    }
}

void term_disable() {
    // framebuffer is no more
    m_framebuffer_pitch = 0;
    m_framebuffer_height = 0;
    m_framebuffer = NULL;
}

