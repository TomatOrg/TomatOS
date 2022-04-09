#pragma once

#include "util/except.h"

void init_gdt();

err_t init_tss();
