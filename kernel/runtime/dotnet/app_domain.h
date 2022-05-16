#pragma once

#include "mir/mir.h"
#include "types.h"

typedef struct app_domain {
    MIR_context_t context;

    System_Reflection_MethodInfo EntryPoint;
} app_domain_t;

app_domain_t* create_app_domain();

void app_domain_load(app_domain_t* app, System_Reflection_Assembly assembly);

typedef struct method_result {
    System_Exception exception;
    uintptr_t result;
} method_result_t;

method_result_t app_domain_link_and_start(app_domain_t* app);

void free_app_domain(app_domain_t* app);

#define FREE_APP_DOMAIN(x) \
    do { \
        if (!(x)) { \
            free_app_domain(x); \
            x = NULL; \
        } \
    } while (0)
