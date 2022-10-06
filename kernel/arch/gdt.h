#pragma once

#include "util/except.h"

#define TSS_ALLOC_SIZE (104 + SIZE_8KB * 4)

void init_gdt();

void init_tss(void* tss_memory);
