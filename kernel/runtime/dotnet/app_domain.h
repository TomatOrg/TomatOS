#pragma once

#include "mir/mir.h"
#include "types.h"

typedef struct app_domain {
    MIR_context_t context;
} app_domain_t;

app_domain_t* create_app_domain();

void app_domain_load(app_domain_t* app, System_Reflection_Assembly assembly);

void app_domain_link(app_domain_t* app);

void free_app_domain(app_domain_t* app);

#define FREE_APP_DOMAIN(x) \
    do { \
        if (!(x)) { \
            free_app_domain(x); \
            x = NULL; \
        } \
    } while (0)
