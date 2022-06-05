#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "util/except.h"

void free_monitor(void* object);

err_t monitor_enter(void* object);

err_t monitor_exit(void* object);
