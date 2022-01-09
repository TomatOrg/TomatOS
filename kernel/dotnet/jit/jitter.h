#pragma once

#include <util/except.h>
#include <dotnet/dotnet.h>

struct jitter_context;
typedef struct jitter_context jitter_context_t;

jitter_context_t* create_jitter();

err_t jitter_jit_method(jitter_context_t* ctx, method_info_t method);

void destroy_jitter(jitter_context_t* jitter);
