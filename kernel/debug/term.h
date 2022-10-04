#pragma once

#include <stdint.h>
#include <stddef.h>

void term_init(uint32_t* framebuffer, size_t width, size_t height, size_t pitch);

void term_print_char(char c);

void term_clear();

void term_disable();
