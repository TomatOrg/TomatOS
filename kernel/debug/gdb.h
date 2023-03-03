#pragma once

#include <util/except.h>
#include "arch/idt.h"

void gdb_handle_exception(exception_context_t* ctx);

void gdb_enter();

err_t init_gdb();
